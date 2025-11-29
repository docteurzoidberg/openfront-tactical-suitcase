import type { GameState, GameEventType } from '../../ots-shared/src/game'
import { BaseWindow, FloatingPanel } from './hud/window'

declare function GM_getValue<T = unknown>(key: string, defaultValue?: T): T
declare function GM_setValue<T = unknown>(key: string, value: T): void

const DEFAULT_WS_URL = 'ws://localhost:3000/ws-script'
const STORAGE_KEY_WS_URL = 'ots-ws-url'
const STORAGE_KEY_HUD_POS = 'ots-hud-pos'
const STORAGE_KEY_HUD_SIZE = 'ots-hud-size'

type HudPos = { left: number; top: number }
type HudSize = { width: number; height: number }

type WsStatus = 'DISCONNECTED' | 'CONNECTING' | 'OPEN' | 'ERROR'
type LogDirection = 'send' | 'recv' | 'info'

type LogEntry = {
  id: number
  ts: number
  direction: LogDirection
  text: string
}

function debugLog(...args: unknown[]) {
  // eslint-disable-next-line no-console
  console.log('[OTS Userscript]', ...args)
}

class Hud {
  private root: HTMLDivElement | null = null
  private closed = false
  private logList: HTMLDivElement | null = null
  private wsDot: HTMLSpanElement | null = null
  private gameDot: HTMLSpanElement | null = null
  private body: HTMLDivElement | null = null
  private settingsPanel: HTMLDivElement | null = null
  private settingsInput: HTMLInputElement | null = null
  private logCounter = 0

  constructor(private getWsUrl: () => string, private setWsUrl: (url: string) => void, private onWsUrlChanged: () => void) { }

  ensure() {
    if (this.closed || this.root) return

    const root = document.createElement('div')
    root.id = 'ots-userscript-hud'
    root.style.position = 'fixed'
    root.style.top = '8px'
    root.style.right = '8px'
    root.style.zIndex = '2147483647'
    root.style.fontFamily = 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    root.style.color = '#e5e7eb'

    root.innerHTML = `
    <div id="ots-hud-inner" style="width: 380px; height: 260px; max-height: 60vh; background: rgba(15,23,42,0.96); border-radius: 8px; box-shadow: 0 8px 20px rgba(0,0,0,0.5); border: 1px solid rgba(148,163,184,0.5); display: flex; flex-direction: column; overflow: hidden; position: relative;">
      <div id="ots-hud-header" style="cursor: move; padding: 6px 8px; display: flex; align-items: center; justify-content: space-between; gap: 6px; background: rgba(15,23,42,0.98); border-bottom: 1px solid rgba(30,64,175,0.7);">
        <div style="display:flex;align-items:center;gap:6px;min-width:0;">
          <span style="display:inline-flex;align-items:center;gap:4px;padding:2px 6px;border-radius:999px;border:1px solid rgba(148,163,184,0.7);background:rgba(15,23,42,0.9);font-size:9px;font-weight:600;letter-spacing:0.09em;text-transform:uppercase;color:#e5e7eb;white-space:nowrap;">
            <span id="ots-hud-ws-dot" style="display:inline-flex;width:8px;height:8px;border-radius:999px;background:#f97373;"></span>
            <span>WS</span>
          </span>
          <span style="display:inline-flex;align-items:center;gap:4px;padding:2px 6px;border-radius:999px;border:1px solid rgba(148,163,184,0.7);background:rgba(15,23,42,0.05);font-size:9px;font-weight:600;letter-spacing:0.09em;text-transform:uppercase;color:#e5e7eb;white-space:nowrap;">
            <span id="ots-hud-game-dot" style="display:inline-flex;width:8px;height:8px;border-radius:999px;background:#f97373;"></span>
            <span>GAME</span>
          </span>
          <span style="font-size:11px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;color:#9ca3af;white-space:nowrap;text-overflow:ellipsis;overflow:hidden;">Openfront Tactical Suitcase link</span>
        </div>
        <div style="display:flex;align-items:center;gap:4px;">
          <button id="ots-hud-toggle" style="all:unset;cursor:pointer;font-size:11px;color:#9ca3af;padding:2px 4px;border-radius:4px;border:1px solid rgba(148,163,184,0.6);background:rgba(15,23,42,0.9);">–</button>
          <button id="ots-hud-settings" style="all:unset;cursor:pointer;font-size:11px;color:#9ca3af;padding:2px 4px;border-radius:4px;border:1px solid rgba(148,163,184,0.6);background:rgba(15,23,42,0.9);">⚙</button>
          <button id="ots-hud-close" style="all:unset;cursor:pointer;font-size:11px;color:#6b7280;padding:2px 4px;border-radius:4px;">✕</button>
        </div>
      </div>
      <div id="ots-hud-body" style="display:flex;flex-direction:column;flex:1 1 auto;min-height:0;">
        <div id="ots-hud-log" style="flex:1 1 auto; padding: 4px 6px 6px; overflow-y:auto; overflow-x:hidden; font-size:11px; line-height:1.3; background:rgba(15,23,42,0.98);">
        </div>
      </div>
      <div id="ots-hud-settings-panel" style="position:fixed;top:64px;right:16px;min-width:260px;max-width:320px;display:none;flex-direction:column;gap:6px;padding:8px;border-radius:6px;background:rgba(15,23,42,0.98);border:1px solid rgba(59,130,246,0.6);box-shadow:0 8px 20px rgba(0,0,0,0.5);z-index:2147483647;">
        <div id="ots-hud-settings-header" style="display:flex;align-items:center;justify-content:space-between;gap:6px;cursor:move;">
          <span style="font-size:11px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;color:#9ca3af;">Settings</span>
          <button id="ots-hud-settings-close" style="all:unset;cursor:pointer;font-size:11px;color:#6b7280;padding:2px 4px;border-radius:4px;">✕</button>
        </div>
        <label style="display:flex;flex-direction:column;gap:4px;font-size:11px;color:#e5e7eb;">
          <span>WebSocket URL</span>
          <input id="ots-hud-settings-ws" type="text" style="font-size:11px;padding:4px 6px;border-radius:4px;border:1px solid rgba(148,163,184,0.8);background:rgba(15,23,42,0.9);color:#e5e7eb;outline:none;" />
        </label>
        <div style="display:flex;justify-content:flex-end;gap:6px;margin-top:4px;">
          <button id="ots-hud-settings-reset" style="all:unset;cursor:pointer;font-size:11px;color:#9ca3af;padding:3px 6px;border-radius:4px;border:1px solid rgba(148,163,184,0.5);background:rgba(15,23,42,0.9);">Reset</button>
          <button id="ots-hud-settings-save" style="all:unset;cursor:pointer;font-size:11px;color:#0f172a;padding:3px 8px;border-radius:4px;background:#4ade80;font-weight:600;">Save</button>
        </div>
      </div>
      <div id="ots-hud-resize" style="position:absolute;right:2px;bottom:2px;width:12px;height:12px;cursor:se-resize;opacity:0.7;">
        <div style="position:absolute;right:1px;bottom:1px;width:10px;height:2px;background:rgba(148,163,184,0.9);transform:rotate(45deg);"></div>
        <div style="position:absolute;right:3px;bottom:3px;width:8px;height:2px;background:rgba(148,163,184,0.7);transform:rotate(45deg);"></div>
      </div>
    </div>
    `

    document.body.appendChild(root)
    this.root = root
    this.logList = root.querySelector('#ots-hud-log') as HTMLDivElement
    this.body = root.querySelector('#ots-hud-body') as HTMLDivElement
    this.wsDot = root.querySelector('#ots-hud-ws-dot') as HTMLSpanElement
    this.gameDot = root.querySelector('#ots-hud-game-dot') as HTMLSpanElement
    this.settingsPanel = root.querySelector('#ots-hud-settings-panel') as HTMLDivElement
    this.settingsInput = root.querySelector('#ots-hud-settings-ws') as HTMLInputElement

    const header = root.querySelector('#ots-hud-header') as HTMLDivElement
    const settingsHeader = root.querySelector('#ots-hud-settings-header') as HTMLDivElement | null
    const inner = root.querySelector('#ots-hud-inner') as HTMLDivElement

    const savedPos = GM_getValue<HudPos | null>(STORAGE_KEY_HUD_POS, null)
    if (savedPos && typeof savedPos.left === 'number' && typeof savedPos.top === 'number') {
      root.style.left = `${savedPos.left}px`
      root.style.top = `${savedPos.top}px`
      root.style.right = 'auto'
    }

    const savedSize = GM_getValue<HudSize | null>(STORAGE_KEY_HUD_SIZE, null)
    if (savedSize && typeof savedSize.width === 'number' && typeof savedSize.height === 'number') {
      inner.style.width = `${savedSize.width}px`
      inner.style.height = `${savedSize.height}px`
    }

    const resizeHandle = root.querySelector('#ots-hud-resize') as HTMLDivElement
    const toggleBtn = root.querySelector('#ots-hud-toggle') as HTMLButtonElement
    const settingsBtn = root.querySelector('#ots-hud-settings') as HTMLButtonElement
    const settingsCloseBtn = root.querySelector('#ots-hud-settings-close') as HTMLButtonElement
    const settingsSaveBtn = root.querySelector('#ots-hud-settings-save') as HTMLButtonElement
    const settingsResetBtn = root.querySelector('#ots-hud-settings-reset') as HTMLButtonElement
    const closeBtn = root.querySelector('#ots-hud-close') as HTMLButtonElement

    const hudWindow = new BaseWindow(root)
    hudWindow.attachDrag(header, (left, top) => {
      const pos: HudPos = { left, top }
      GM_setValue(STORAGE_KEY_HUD_POS, pos)
    })

    if (this.settingsPanel && settingsHeader) {
      const settingsWindow = new FloatingPanel(this.settingsPanel)
      settingsWindow.attachDrag(settingsHeader)
    }

    setupResize(inner, resizeHandle)

    let collapsed = false
    toggleBtn.addEventListener('click', () => {
      if (!this.body) return
      collapsed = !collapsed
      this.body.style.display = collapsed ? 'none' : 'flex'
      if (collapsed) {
        inner.style.height = '32px'
      } else {
        inner.style.height = ''
      }
      toggleBtn.textContent = collapsed ? '+' : '–'
    })

    settingsBtn.addEventListener('click', () => {
      if (!this.settingsPanel || !this.settingsInput) return
      this.settingsPanel.style.display = 'flex'
      this.settingsInput.value = this.getWsUrl()
      this.settingsInput.focus()
    })

    settingsCloseBtn.addEventListener('click', () => {
      if (this.settingsPanel) this.settingsPanel.style.display = 'none'
    })

    settingsResetBtn.addEventListener('click', () => {
      if (!this.settingsInput) return
      this.settingsInput.value = DEFAULT_WS_URL
    })

    settingsSaveBtn.addEventListener('click', () => {
      if (!this.settingsInput) return
      const value = this.settingsInput.value.trim()
      if (!value) return
      this.setWsUrl(value)
      this.pushLog('info', `WS URL updated to ${value}`)
      if (this.settingsPanel) this.settingsPanel.style.display = 'none'
      this.onWsUrlChanged()
    })

    closeBtn.addEventListener('click', () => {
      root.remove()
      this.root = null
      this.logList = null
      this.wsDot = null
      this.gameDot = null
      this.closed = true
    })
  }

  setWsStatus(status: WsStatus) {
    this.ensure()
    if (!this.wsDot) return
    let color = '#f97373'
    if (status === 'CONNECTING') color = '#fbbf24'
    if (status === 'OPEN') color = '#4ade80'
    if (status === 'ERROR') color = '#f97373'
    this.wsDot.style.background = color
  }

  setGameStatus(connected: boolean) {
    this.ensure()
    if (!this.gameDot) return
    this.gameDot.style.background = connected ? '#4ade80' : '#f97373'
  }

  pushLog(direction: LogDirection, text: string) {
    this.ensure()
    if (!this.logList) return

    const entry: LogEntry = {
      id: ++this.logCounter,
      ts: Date.now(),
      direction,
      text
    }

    const line = document.createElement('div')
    line.style.display = 'flex'
    line.style.gap = '4px'
    line.style.marginBottom = '2px'

    const dirSpan = document.createElement('span')
    dirSpan.textContent = direction.toUpperCase()
    dirSpan.style.fontSize = '9px'
    dirSpan.style.fontWeight = '600'
    dirSpan.style.letterSpacing = '0.08em'
    dirSpan.style.textTransform = 'uppercase'
    dirSpan.style.padding = '1px 4px'
    dirSpan.style.borderRadius = '999px'

    if (direction === 'send') {
      dirSpan.style.background = 'rgba(52,211,153,0.18)'
      dirSpan.style.color = '#6ee7b7'
    } else if (direction === 'recv') {
      dirSpan.style.background = 'rgba(59,130,246,0.22)'
      dirSpan.style.color = '#93c5fd'
    } else {
      dirSpan.style.background = 'rgba(156,163,175,0.25)'
      dirSpan.style.color = '#e5e7eb'
    }

    const textSpan = document.createElement('span')
    textSpan.textContent = text
    textSpan.style.flex = '1 1 auto'
    textSpan.style.color = '#e5e7eb'
    textSpan.style.whiteSpace = 'nowrap'
    textSpan.style.textOverflow = 'ellipsis'
    textSpan.style.overflow = 'hidden'

    line.appendChild(dirSpan)
    line.appendChild(textSpan)

    this.logList.appendChild(line)
    this.logList.scrollTop = this.logList.scrollHeight
  }
}

function setupResize(container: HTMLDivElement, handle: HTMLDivElement) {
  let resizing = false
  let startX = 0
  let startY = 0
  let startWidth = 0
  let startHeight = 0

  handle.addEventListener('mousedown', (ev) => {
    resizing = true
    startX = ev.clientX
    startY = ev.clientY
    const rect = container.getBoundingClientRect()
    startWidth = rect.width
    startHeight = rect.height
    ev.preventDefault()
    ev.stopPropagation()
  })

  window.addEventListener('mousemove', (ev) => {
    if (!resizing) return
    const dx = ev.clientX - startX
    const dy = ev.clientY - startY
    const newWidth = Math.max(220, startWidth + dx)
    const newHeight = Math.max(140, startHeight + dy)
    container.style.width = `${newWidth}px`
    container.style.height = `${newHeight}px`

    const size: HudSize = { width: newWidth, height: newHeight }
    GM_setValue(STORAGE_KEY_HUD_SIZE, size)
  })

  window.addEventListener('mouseup', () => {
    resizing = false
  })
}

class WsClient {
  private socket: WebSocket | null = null
  private reconnectTimeout: number | null = null
  private reconnectDelay = 2000

  constructor(private hud: Hud, private getWsUrl: () => string) { }

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
      this.sendInfo('userscript-connected', { url: window.location.href })
    })

    this.socket.addEventListener('close', (ev) => {
      debugLog('WebSocket closed', ev.code, ev.reason)
      this.hud.setWsStatus('DISCONNECTED')
      this.hud.pushLog('info', `WebSocket closed (${ev.code} ${ev.reason || ''})`)
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
      }

      if (action.startsWith('focus-player:')) {
        const playerId = action.split(':')[1]
        this.sendInfo('focus-player-received', { playerId })
        // TODO: implement game specific focus logic
      }
    }
  }
}

type DashboardCommand = {
  type: 'cmd'
  payload: { action: string; params?: unknown }
}

// demoHook removed: userscript now relies solely on real game data

// ---- Game hook integration (from OpenFrontIO sample) ----

const CONTROL_PANEL_SELECTOR = 'control-panel'
const GAME_UPDATE_EVENT = 'openfront:game-update'
const GAME_CHECK_INTERVAL = 100
const GAME_UPDATE_TYPE_UNIT = 1

let gameUpdateListenerAttached = false
let initialPayloadSent = false

function waitForElement(selector: string, callback: (el: Element) => void) {
  const existing = document.querySelector(selector)
  if (existing) {
    callback(existing)
    return
  }

  const observer = new MutationObserver(() => {
    const el = document.querySelector(selector)
    if (el) {
      observer.disconnect()
      callback(el)
    }
  })

  observer.observe(document.documentElement || document.body, {
    childList: true,
    subtree: true
  })
}

function waitForGameInstance(controlPanel: any, callback: (game: any) => void) {
  const existing = (controlPanel as any).game
  if (existing) {
    callback(existing)
    return
  }

  const intervalId = window.setInterval(() => {
    const game = (controlPanel as any).game
    if (game) {
      window.clearInterval(intervalId)
      callback(game)
    }
  }, GAME_CHECK_INTERVAL)
}

function hookGameUpdates(game: any) {
  if (!game) {
    debugLog('[OTS] No GameView instance to hook.')
    return
  }

  const proto = Object.getPrototypeOf(game)
  if (!proto || typeof proto.update !== 'function') {
    debugLog('[OTS] GameView prototype missing update method; skipping hook.')
    return
  }

  if (proto.__openfrontGameHooked) {
    return
  }

  const originalUpdate = proto.update
  proto.update = function patchedGameUpdate(this: any, ...args: any[]) {
    const result = originalUpdate.apply(this, args)
    window.dispatchEvent(
      new CustomEvent(GAME_UPDATE_EVENT, {
        detail: { game: this, payload: args[0] }
      })
    )
    return result
  }

  Object.defineProperty(proto, '__openfrontGameHooked', {
    value: true,
    configurable: true
  })

  debugLog('[OTS] Hooked GameView.update().')
  attachGameUpdateListener()
}

function attachGameUpdateListener() {
  if (gameUpdateListenerAttached) {
    return
  }

  window.addEventListener(GAME_UPDATE_EVENT, handleGameUpdate as EventListener)
  gameUpdateListenerAttached = true
}

function handleGameUpdate(event: CustomEvent<{ game: any; payload: any }>) {
  const game = event?.detail?.game
  const viewData = event?.detail?.payload
  if (!game || !viewData || !viewData.updates) {
    return
  }

  // On first update, send the full raw payload once so the
  // server/dashboard can inspect the structure.
  if (!initialPayloadSent) {
    sendEvent('INFO', 'of-initial-gameview-payload', {
      game,
      payload: viewData
    })
    initialPayloadSent = true
  }

  const myPlayer = typeof game.myPlayer === 'function' ? game.myPlayer() : null
  const mySmallID =
    typeof myPlayer?.smallID === 'function' ? myPlayer.smallID() : undefined
  if (mySmallID === undefined) {
    return
  }

  const unitUpdates = viewData.updates[GAME_UPDATE_TYPE_UNIT] ?? []
  const myUnits = unitUpdates.filter((u: any) => u.ownerID === mySmallID)

  if (!myUnits.length) return

  sendEvent('INFO', 'of-game-unit-updates', {
    count: myUnits.length,
    sample: myUnits.slice(0, 3).map((u: any) => ({
      id: u.id,
      type: u.unitType,
      ownerID: u.ownerID,
      troops: u.troops,
      underConstruction: u.underConstruction,
      isActive: u.isActive
    }))
  })
}

class GameBridge {
  constructor(private ws: WsClient, private hud: Hud) { }

  init() {
    waitForElement(CONTROL_PANEL_SELECTOR, (controlPanel) => {
      debugLog('[OTS] control panel detected', controlPanel)
      waitForGameInstance(controlPanel, (game) => {
        ; (window as any).openfrontControlPanelGame = game
        debugLog('[OTS] GameView ready', game)
        this.ws.sendEvent('INFO', 'of-game-instance-detected', game)
        this.hud.setGameStatus(true)
        hookGameUpdates(game)
      })
    })
  }
}

function loadWsUrl(): string {
  const saved = GM_getValue<string | null>(STORAGE_KEY_WS_URL, null)
  if (typeof saved === 'string' && saved.trim()) {
    return saved
  }
  return DEFAULT_WS_URL
}

function saveWsUrl(url: string) {
  GM_setValue(STORAGE_KEY_WS_URL, url)
}

; (function start() {
  let currentWsUrl = loadWsUrl()

  const hud = new Hud(
    () => currentWsUrl,
    (url) => {
      currentWsUrl = url
      saveWsUrl(url)
    },
    () => {
      ws.disconnect(4100, 'URL changed')
      ws.connect()
    }
  )

  const ws = new WsClient(hud, () => currentWsUrl)
  const game = new GameBridge(ws, hud)

  hud.ensure()
  ws.connect()
  game.init()
    ; (window as any).otsShowHud = () => hud.ensure()
})()
