import type { GameState, GameEventType } from '../../../ots-shared/src/game'
import type { Hud } from '../hud/main-hud'

type WsStatus = 'DISCONNECTED' | 'CONNECTING' | 'OPEN' | 'ERROR'

type DashboardCommand = {
  type: 'cmd'
  payload: { action: string; params?: unknown }
}

function debugLog(...args: unknown[]) {
  // eslint-disable-next-line no-console
  console.log('[OTS Userscript]', ...args)
}

export class WsClient {
  private socket: WebSocket | null = null
  private reconnectTimeout: number | null = null
  private reconnectDelay = 2000
  private heartbeatInterval: number | null = null

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

    const url = this.getWsUrl()
    debugLog('Connecting to', url)
    this.hud.setWsStatus('CONNECTING')
    this.hud.pushLog('info', `Connecting to ${url}`)
    this.socket = new WebSocket(url)

    this.socket.addEventListener('open', () => {
      debugLog('WebSocket connected')
      this.hud.setWsStatus('OPEN')
      this.hud.pushLog('info', 'WebSocket connected')
      this.reconnectDelay = 2000

      // Send handshake to identify as userscript client
      this.safeSend({ type: 'handshake', clientType: 'userscript' })

      this.sendInfo('userscript-connected', { url: window.location.href })

      // Start periodic heartbeat (every 5 seconds)
      this.startHeartbeat()
    })

    this.socket.addEventListener('close', (ev) => {
      debugLog('WebSocket closed', ev.code, ev.reason)
      this.hud.setWsStatus('DISCONNECTED')
      this.hud.pushLog('info', `WebSocket closed (${ev.code} ${ev.reason || ''})`)
      this.stopHeartbeat()
      this.scheduleReconnect()
    })

    this.socket.addEventListener('error', (err) => {
      debugLog('WebSocket error', err)
      this.hud.setWsStatus('ERROR')
      this.hud.pushLog('info', 'WebSocket error')
      this.scheduleReconnect()
    })

    this.socket.addEventListener('message', (event) => {
      this.hud.pushLog('recv', typeof event.data === 'string' ? event.data : '[binary message]')
      this.handleServerMessage(event.data)
    })
  }

  disconnect(code?: number, reason?: string) {
    if (!this.socket) return
    this.stopHeartbeat()
    try {
      this.socket.close(code, reason)
    } catch {
      // ignore
    }
  }

  private scheduleReconnect() {
    if (this.reconnectTimeout !== null) return
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
    this.hud.pushLog('send', json)
    this.socket.send(json)
  }

  sendState(state: GameState) {
    this.safeSend({
      type: 'state',
      payload: state
    })
  }

  sendEvent(type: GameEventType, message: string, data?: unknown) {
    this.safeSend({
      type: 'event',
      payload: {
        type,
        timestamp: Date.now(),
        message,
        data
      }
    })
  }

  sendInfo(message: string, data?: unknown) {
    this.sendEvent('INFO', message, data)
  }

  private startHeartbeat() {
    this.stopHeartbeat()
    this.heartbeatInterval = window.setInterval(() => {
      this.sendInfo('heartbeat')
    }, 5000)
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

    let msg: DashboardCommand
    try {
      msg = JSON.parse(raw) as DashboardCommand
    } catch (e) {
      debugLog('Invalid JSON from server', raw, e)
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
