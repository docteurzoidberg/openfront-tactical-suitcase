import { BaseWindow, FloatingPanel } from './window'

declare function GM_getValue<T = unknown>(key: string, defaultValue?: T): T
declare function GM_setValue<T = unknown>(key: string, value: T): void

const STORAGE_KEY_HUD_POS = 'ots-hud-pos'
const STORAGE_KEY_HUD_SIZE = 'ots-hud-size'

type HudPos = { left: number; top: number }
type HudSize = { width: number; height: number }

export type WsStatus = 'DISCONNECTED' | 'CONNECTING' | 'OPEN' | 'ERROR'
export type LogDirection = 'send' | 'recv' | 'info'

type LogEntry = {
  id: number
  ts: number
  direction: LogDirection
  text: string
}

export class Hud {
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

    root.innerHTML = `<div id="ots-hud-inner" style="width: 380px; height: 260px; max-height: 60vh; background: rgba(15,23,42,0.96); border-radius: 8px; box-shadow: 0 8px 20px rgba(0,0,0,0.5); border: 1px solid rgba(148,163,184,0.5); display: flex; flex-direction: column; overflow: hidden; position: relative;">
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
    </div>`

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
      this.settingsInput.value = this.getWsUrl()
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
