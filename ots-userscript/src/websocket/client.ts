import type {
  CmdMessage,
  EventMessage,
  GameEventType,
  GameState,
  HandshakeMessage,
  StateMessage,
  WsMessage,
  WsStatus
} from '../../../ots-shared/src/game'
import { PROTOCOL_CONSTANTS } from '../../../ots-shared/src/game'
import type { Hud } from '../hud/sidebar-hud'
import { DEFAULT_RECONNECT_DELAY_MS, MAX_RECONNECT_DELAY_MS } from '../game/constants'

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null
}

function parseWsMessage(raw: string): WsMessage | null {
  try {
    const parsed: unknown = JSON.parse(raw)
    if (!isRecord(parsed)) return null
    if (typeof parsed.type !== 'string') return null
    return parsed as WsMessage
  } catch {
    return null
  }
}

function debugLog(...args: unknown[]) {
  // eslint-disable-next-line no-console
  console.log('[OTS Userscript]', ...args)
}

export class WsClient {
  private socket: WebSocket | null = null
  private reconnectTimeout: number | null = null
  private reconnectDelay = DEFAULT_RECONNECT_DELAY_MS
  private heartbeatInterval: number | null = null
  private shouldReconnect = true

  constructor(
    private hud: Hud,
    private getWsUrl: () => string,
    private onCommand?: (action: string, params?: unknown) => void
  ) { }

  connect() {
    if (this.socket && this.socket.readyState === WebSocket.OPEN) {
      return
    }

    if (this.socket && this.socket.readyState === WebSocket.CONNECTING) {
      return
    }

    // Clear any pending reconnect timeout
    if (this.reconnectTimeout !== null) {
      window.clearTimeout(this.reconnectTimeout)
      this.reconnectTimeout = null
    }

    const url = this.getWsUrl()
    debugLog('Connecting to', url)
    this.hud.setWsStatus('CONNECTING')
    this.hud.pushLog('info', `Connecting to ${url}`)
    this.shouldReconnect = true

    try {
      this.socket = new WebSocket(url)
    } catch (error) {
      debugLog('Failed to create WebSocket:', error)
      this.hud.setWsStatus('ERROR')
      this.hud.pushLog('info', `Failed to create WebSocket: ${error}`)
      this.scheduleReconnect()
      return
    }

    this.socket.addEventListener('open', () => {
      debugLog('WebSocket connected')
      this.hud.setWsStatus('OPEN')
      this.hud.pushLog('info', 'WebSocket connected')
      this.reconnectDelay = DEFAULT_RECONNECT_DELAY_MS

      // Send handshake to identify as userscript client
      const handshake: HandshakeMessage = {
        type: 'handshake',
        clientType: PROTOCOL_CONSTANTS.CLIENT_TYPE_USERSCRIPT
      }
      this.safeSend(handshake)

      this.sendInfo(PROTOCOL_CONSTANTS.INFO_MESSAGE_USERSCRIPT_CONNECTED, { url: window.location.href })

      // Start periodic heartbeat (every 15 seconds) for stale detection
      this.startHeartbeat()
    })

    this.socket.addEventListener('close', (ev) => {
      debugLog('WebSocket closed', ev.code, ev.reason)
      this.hud.setWsStatus('DISCONNECTED')
      this.hud.pushLog('info', `WebSocket closed (${ev.code} ${ev.reason || ''})`)
      this.stopHeartbeat()
      if (this.shouldReconnect) {
        this.scheduleReconnect()
      }
    })

    this.socket.addEventListener('error', (err) => {
      debugLog('WebSocket error', err)
      this.hud.setWsStatus('ERROR')
      this.hud.pushLog('info', 'WebSocket error - will retry connection')
      // Note: 'close' event will be fired after 'error', so reconnect happens there
    })

    this.socket.addEventListener('message', (event) => {
      // Try to extract event type from message for filtering
      let eventType: GameEventType | undefined
      if (typeof event.data === 'string') {
        const parsed = parseWsMessage(event.data)
        if (parsed?.type === 'event') {
          eventType = (parsed as EventMessage).payload.type
        }
      }

      this.hud.pushLog('recv', typeof event.data === 'string' ? event.data : '[binary message]', eventType)
      this.handleServerMessage(event.data)
    })
  }

  disconnect(code?: number, reason?: string) {
    if (!this.socket) return
    this.shouldReconnect = false
    this.stopHeartbeat()
    if (this.reconnectTimeout !== null) {
      window.clearTimeout(this.reconnectTimeout)
      this.reconnectTimeout = null
    }
    try {
      this.socket.close(code, reason)
    } catch {
      // ignore
    }
  }

  private scheduleReconnect() {
    if (this.reconnectTimeout !== null) return
    if (!this.shouldReconnect) return

    debugLog(`Scheduling reconnect in ${this.reconnectDelay}ms`)
    this.reconnectTimeout = window.setTimeout(() => {
      this.reconnectTimeout = null
      this.reconnectDelay = Math.min(this.reconnectDelay * 1.5, 15000)
      if (this.socket && this.socket.readyState !== WebSocket.OPEN) {
        try {
          this.socket.close()
        } catch {
          // ignore
        }
        this.socket = null
      }
      debugLog('Attempting to reconnect...')
      this.connect()
    }, this.reconnectDelay)
  }

  private safeSend(msg: unknown) {
    if (!this.socket || this.socket.readyState !== WebSocket.OPEN) {
      debugLog('Cannot send, socket not open')
      this.hud.pushLog('info', 'Cannot send, socket not open')
      return
    }
    const json = JSON.stringify(msg)

    // Extract event type for filtering if it's an event message
    let eventType: GameEventType | undefined
    if (isRecord(msg) && msg.type === 'event') {
      const payload = msg.payload
      if (isRecord(payload) && typeof payload.type === 'string') {
        eventType = payload.type as GameEventType
      }
    }

    this.hud.pushLog('send', json, eventType)
    this.socket.send(json)
  }

  sendState(state: GameState) {
    const msg: StateMessage = { type: 'state', payload: state }
    this.safeSend(msg)
  }

  sendEvent(type: GameEventType, message: string, data?: unknown) {
    const msg: EventMessage = {
      type: 'event',
      payload: {
        type,
        timestamp: Date.now(),
        message,
        data
      }
    }
    this.safeSend(msg)
  }

  sendInfo(message: string, data?: unknown) {
    this.sendEvent('INFO', message, data)
  }

  sendCommand(action: string, params?: unknown) {
    const msg: CmdMessage = { type: 'cmd', payload: { action, params } }
    this.safeSend(msg)
  }

  private startHeartbeat() {
    this.stopHeartbeat()
    this.heartbeatInterval = window.setInterval(() => {
      this.sendInfo('heartbeat')
    }, 15000) // Send heartbeat every 15 seconds
  }

  private stopHeartbeat() {
    if (this.heartbeatInterval !== null) {
      window.clearInterval(this.heartbeatInterval)
      this.heartbeatInterval = null
    }
  }

  private handleServerMessage(raw: unknown) {
    if (typeof raw !== 'string') {
      debugLog('Non-text message from server', raw)
      this.hud.pushLog('info', 'Non-text message from server')
      return
    }

    const msg = parseWsMessage(raw)
    if (!msg) {
      debugLog('Invalid JSON from server', raw)
      this.hud.pushLog('info', 'Invalid JSON from server')
      return
    }

    if (msg.type === 'cmd' && typeof msg.payload?.action === 'string') {
      const { action, params } = msg.payload
      debugLog('Received command from dashboard:', action, params)

      if (action === 'ping') {
        this.sendInfo('pong-from-userscript')
      } else if (this.onCommand) {
        // Forward command to the game bridge
        this.onCommand(action, params)
      } else {
        debugLog('No command handler registered for:', action)
      }
    }
  }
}
