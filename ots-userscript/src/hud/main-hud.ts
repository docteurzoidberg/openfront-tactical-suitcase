import { BaseWindow, FloatingPanel } from './window'

declare function GM_getValue<T = unknown>(key: string, defaultValue?: T): T
declare function GM_setValue<T = unknown>(key: string, value: T): void

const STORAGE_KEY_HUD_POS = 'ots-hud-pos'
const STORAGE_KEY_HUD_SIZE = 'ots-hud-size'
const STORAGE_KEY_LOG_FILTERS = 'ots-hud-log-filters'

import type { WsStatus, GameEventType } from '../../../ots-shared/src/game'

type HudPos = { left: number; top: number }
type HudSize = { width: number; height: number }
type LogFilters = {
  directions: { send: boolean; recv: boolean; info: boolean }
  events: {
    game: boolean        // GAME_START, GAME_END, GAME_SPAWNING
    nukes: boolean       // NUKE_LAUNCHED, NUKE_EXPLODED, NUKE_INTERCEPTED
    alerts: boolean      // ALERT_NUKE, ALERT_HYDRO, ALERT_MIRV, ALERT_LAND, ALERT_NAVAL
    troops: boolean      // TROOP_UPDATE
    system: boolean      // INFO, ERROR, HARDWARE_TEST
  }
}

export type { WsStatus }
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
  private filterPanel: HTMLDivElement | null = null
  private filterCheckboxes = {
    directions: { send: null as HTMLInputElement | null, recv: null as HTMLInputElement | null, info: null as HTMLInputElement | null },
    events: { game: null as HTMLInputElement | null, nukes: null as HTMLInputElement | null, alerts: null as HTMLInputElement | null, troops: null as HTMLInputElement | null, system: null as HTMLInputElement | null }
  }
  private logFilters: LogFilters = {
    directions: { send: true, recv: true, info: true },
    events: { game: true, nukes: true, alerts: true, troops: true, system: true }
  }
  private logCounter = 0
  private autoScroll = true
  private autoScrollBtn: HTMLButtonElement | null = null

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

    root.innerHTML = `<div id="ots-hud-inner" style="width: 320px; height: 260px; max-height: 60vh; background: rgba(15,23,42,0.96); border-radius: 8px; box-shadow: 0 8px 20px rgba(0,0,0,0.5); border: 1px solid rgba(148,163,184,0.5); display: flex; flex-direction: column; overflow: hidden; position: relative;">
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
          <span style="font-size:11px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;color:#9ca3af;white-space:nowrap;text-overflow:ellipsis;overflow:hidden;">OTS LINK</span>
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
        <div id="ots-hud-footer" style="padding:4px 8px;display:flex;align-items:center;justify-content:space-between;background:rgba(15,23,42,0.98);border-top:1px solid rgba(30,64,175,0.7);">
          <button id="ots-hud-filter" style="all:unset;cursor:pointer;font-size:11px;color:#9ca3af;padding:2px 6px;border-radius:4px;border:1px solid rgba(148,163,184,0.6);background:rgba(15,23,42,0.9);">⚑ Filter</button>
          <button id="ots-hud-autoscroll" style="all:unset;cursor:pointer;font-size:9px;font-weight:600;color:#6ee7b7;padding:2px 6px;border-radius:999px;border:1px solid rgba(52,211,153,0.6);background:rgba(52,211,153,0.18);white-space:nowrap;letter-spacing:0.05em;">✓ Auto</button>
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
      <div id="ots-hud-filter-panel" style="position:fixed;top:64px;right:16px;min-width:220px;max-width:280px;display:none;flex-direction:column;gap:8px;padding:10px;border-radius:6px;background:rgba(15,23,42,0.98);border:1px solid rgba(59,130,246,0.6);box-shadow:0 8px 20px rgba(0,0,0,0.5);z-index:2147483647;">
        <div id="ots-hud-filter-header" style="display:flex;align-items:center;justify-content:space-between;gap:6px;cursor:move;">
          <span style="font-size:11px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;color:#9ca3af;">Log Filters</span>
          <button id="ots-hud-filter-close" style="all:unset;cursor:pointer;font-size:11px;color:#6b7280;padding:2px 4px;border-radius:4px;">✕</button>
        </div>
        <div style="border-bottom:1px solid rgba(148,163,184,0.3);padding-bottom:6px;">
          <div style="font-size:10px;font-weight:600;color:#9ca3af;margin-bottom:6px;letter-spacing:0.05em;">DIRECTION</div>
          <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
            <input type="checkbox" id="ots-hud-filter-send" checked style="cursor:pointer;" />
            <span style="display:inline-flex;font-size:9px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;padding:1px 4px;border-radius:999px;background:rgba(52,211,153,0.18);color:#6ee7b7;">SEND</span>
          </label>
          <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
            <input type="checkbox" id="ots-hud-filter-recv" checked style="cursor:pointer;" />
            <span style="display:inline-flex;font-size:9px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;padding:1px 4px;border-radius:999px;background:rgba(59,130,246,0.22);color:#93c5fd;">RECV</span>
          </label>
          <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;">
            <input type="checkbox" id="ots-hud-filter-info" checked style="cursor:pointer;" />
            <span style="display:inline-flex;font-size:9px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;padding:1px 4px;border-radius:999px;background:rgba(156,163,175,0.25);color:#e5e7eb;">INFO</span>
          </label>
        </div>
        <div>
          <div style="font-size:10px;font-weight:600;color:#9ca3af;margin-bottom:6px;letter-spacing:0.05em;">EVENT TYPE</div>
          <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
            <input type="checkbox" id="ots-hud-filter-game" checked style="cursor:pointer;" />
            <span style="color:#a78bfa;">Game Events</span>
          </label>
          <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
            <input type="checkbox" id="ots-hud-filter-nukes" checked style="cursor:pointer;" />
            <span style="color:#f87171;">Nuke Events</span>
          </label>
          <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
            <input type="checkbox" id="ots-hud-filter-alerts" checked style="cursor:pointer;" />
            <span style="color:#fb923c;">Alert Events</span>
          </label>
          <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
            <input type="checkbox" id="ots-hud-filter-troops" checked style="cursor:pointer;" />
            <span style="color:#34d399;">Troop Events</span>
          </label>
          <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;">
            <input type="checkbox" id="ots-hud-filter-system" checked style="cursor:pointer;" />
            <span style="color:#94a3b8;">System Events</span>
          </label>
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
    const footer = root.querySelector('#ots-hud-footer') as HTMLDivElement
    this.wsDot = root.querySelector('#ots-hud-ws-dot') as HTMLSpanElement
    this.gameDot = root.querySelector('#ots-hud-game-dot') as HTMLSpanElement
    this.settingsPanel = root.querySelector('#ots-hud-settings-panel') as HTMLDivElement
    this.settingsInput = root.querySelector('#ots-hud-settings-ws') as HTMLInputElement
    this.filterPanel = root.querySelector('#ots-hud-filter-panel') as HTMLDivElement
    this.autoScrollBtn = root.querySelector('#ots-hud-autoscroll') as HTMLButtonElement
    this.filterCheckboxes.directions.send = root.querySelector('#ots-hud-filter-send') as HTMLInputElement
    this.filterCheckboxes.directions.recv = root.querySelector('#ots-hud-filter-recv') as HTMLInputElement
    this.filterCheckboxes.directions.info = root.querySelector('#ots-hud-filter-info') as HTMLInputElement
    this.filterCheckboxes.events.game = root.querySelector('#ots-hud-filter-game') as HTMLInputElement
    this.filterCheckboxes.events.nukes = root.querySelector('#ots-hud-filter-nukes') as HTMLInputElement
    this.filterCheckboxes.events.alerts = root.querySelector('#ots-hud-filter-alerts') as HTMLInputElement
    this.filterCheckboxes.events.troops = root.querySelector('#ots-hud-filter-troops') as HTMLInputElement
    this.filterCheckboxes.events.system = root.querySelector('#ots-hud-filter-system') as HTMLInputElement

    // Load saved filters
    const savedFilters = GM_getValue<LogFilters | null>(STORAGE_KEY_LOG_FILTERS, null)
    if (savedFilters) {
      this.logFilters = savedFilters
      // Restore direction checkboxes
      if (this.filterCheckboxes.directions.send) this.filterCheckboxes.directions.send.checked = savedFilters.directions.send
      if (this.filterCheckboxes.directions.recv) this.filterCheckboxes.directions.recv.checked = savedFilters.directions.recv
      if (this.filterCheckboxes.directions.info) this.filterCheckboxes.directions.info.checked = savedFilters.directions.info
      // Restore event checkboxes
      if (this.filterCheckboxes.events.game) this.filterCheckboxes.events.game.checked = savedFilters.events.game
      if (this.filterCheckboxes.events.nukes) this.filterCheckboxes.events.nukes.checked = savedFilters.events.nukes
      if (this.filterCheckboxes.events.alerts) this.filterCheckboxes.events.alerts.checked = savedFilters.events.alerts
      if (this.filterCheckboxes.events.troops) this.filterCheckboxes.events.troops.checked = savedFilters.events.troops
      if (this.filterCheckboxes.events.system) this.filterCheckboxes.events.system.checked = savedFilters.events.system
    }

    const header = root.querySelector('#ots-hud-header') as HTMLDivElement
    const settingsHeader = root.querySelector('#ots-hud-settings-header') as HTMLDivElement | null
    const filterHeader = root.querySelector('#ots-hud-filter-header') as HTMLDivElement | null
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
    const autoScrollBtn = root.querySelector('#ots-hud-autoscroll') as HTMLButtonElement
    const filterBtn = root.querySelector('#ots-hud-filter') as HTMLButtonElement
    const filterCloseBtn = root.querySelector('#ots-hud-filter-close') as HTMLButtonElement
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

    if (this.filterPanel && filterHeader) {
      const filterWindow = new FloatingPanel(this.filterPanel)
      filterWindow.attachDrag(filterHeader)
    }

    setupResize(inner, resizeHandle)

    // Setup autoscroll behavior
    if (this.logList) {
      this.logList.addEventListener('scroll', () => {
        if (!this.logList) return
        const isAtBottom = this.logList.scrollHeight - this.logList.scrollTop - this.logList.clientHeight < 50
        this.autoScroll = isAtBottom
        this.updateAutoScrollButton()
      })
    }

    autoScrollBtn.addEventListener('click', () => {
      this.autoScroll = true
      this.updateAutoScrollButton()
      this.scrollLogToBottom()
    })

    // Start collapsed by default
    let collapsed = true
    let savedHeight = savedSize?.height || 260
    let savedWidth = savedSize?.width || 320
    if (this.body) {
      this.body.style.display = 'none'
      footer.style.display = 'none'
      inner.style.height = '32px'
      inner.style.width = '250px'
      toggleBtn.textContent = '+'
    }

    toggleBtn.addEventListener('click', () => {
      if (!this.body) return
      collapsed = !collapsed
      this.body.style.display = collapsed ? 'none' : 'flex'
      footer.style.display = collapsed ? 'none' : 'flex'
      if (collapsed) {
        // Save current dimensions before collapsing
        const currentHeight = parseInt(inner.style.height || '260')
        const currentWidth = parseInt(inner.style.width || '320')
        if (currentHeight > 32) {
          savedHeight = currentHeight
        }
        if (currentWidth > 250) {
          savedWidth = currentWidth
        }
        inner.style.height = '32px'
        inner.style.width = '250px'
      } else {
        // When expanding, check if window would go off-screen
        const currentTop = parseInt(root.style.top || '8')
        const currentBottom = currentTop + 32  // Current collapsed height
        const expandedBottom = currentTop + savedHeight
        const viewportHeight = window.innerHeight

        // If expanded window would go past bottom of screen, move it up
        if (expandedBottom > viewportHeight) {
          const newTop = Math.max(8, viewportHeight - savedHeight - 8)
          root.style.top = `${newTop}px`
          // Save new position
          GM_setValue(STORAGE_KEY_HUD_POS, {
            top: newTop,
            left: parseInt(root.style.left || '8')
          })
        }

        // Restore saved dimensions when expanding
        inner.style.height = `${savedHeight}px`
        inner.style.width = `${savedWidth}px`
      }
      toggleBtn.textContent = collapsed ? '+' : '–'
    })

    filterBtn.addEventListener('click', () => {
      if (!this.filterPanel) return
      this.filterPanel.style.display = 'flex'
    })

    filterCloseBtn.addEventListener('click', () => {
      if (this.filterPanel) this.filterPanel.style.display = 'none'
    })

    // Add change listeners to direction filter checkboxes
    if (this.filterCheckboxes.directions.send) {
      this.filterCheckboxes.directions.send.addEventListener('change', (e) => {
        this.logFilters.directions.send = (e.target as HTMLInputElement).checked
        GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters)
        this.applyFiltersToLog()
      })
    }
    if (this.filterCheckboxes.directions.recv) {
      this.filterCheckboxes.directions.recv.addEventListener('change', (e) => {
        this.logFilters.directions.recv = (e.target as HTMLInputElement).checked
        GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters)
        this.applyFiltersToLog()
      })
    }
    if (this.filterCheckboxes.directions.info) {
      this.filterCheckboxes.directions.info.addEventListener('change', (e) => {
        this.logFilters.directions.info = (e.target as HTMLInputElement).checked
        GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters)
        this.applyFiltersToLog()
      })
    }

    // Add change listeners to event type filter checkboxes
    if (this.filterCheckboxes.events.game) {
      this.filterCheckboxes.events.game.addEventListener('change', (e) => {
        this.logFilters.events.game = (e.target as HTMLInputElement).checked
        GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters)
        this.applyFiltersToLog()
      })
    }
    if (this.filterCheckboxes.events.nukes) {
      this.filterCheckboxes.events.nukes.addEventListener('change', (e) => {
        this.logFilters.events.nukes = (e.target as HTMLInputElement).checked
        GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters)
        this.applyFiltersToLog()
      })
    }
    if (this.filterCheckboxes.events.alerts) {
      this.filterCheckboxes.events.alerts.addEventListener('change', (e) => {
        this.logFilters.events.alerts = (e.target as HTMLInputElement).checked
        GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters)
        this.applyFiltersToLog()
      })
    }
    if (this.filterCheckboxes.events.troops) {
      this.filterCheckboxes.events.troops.addEventListener('change', (e) => {
        this.logFilters.events.troops = (e.target as HTMLInputElement).checked
        GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters)
        this.applyFiltersToLog()
      })
    }
    if (this.filterCheckboxes.events.system) {
      this.filterCheckboxes.events.system.addEventListener('change', (e) => {
        this.logFilters.events.system = (e.target as HTMLInputElement).checked
        GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters)
        this.applyFiltersToLog()
      })
    }

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

  private updateAutoScrollButton() {
    if (!this.autoScrollBtn) return

    if (this.autoScroll) {
      this.autoScrollBtn.textContent = '✓ Auto'
      this.autoScrollBtn.style.color = '#6ee7b7'
      this.autoScrollBtn.style.background = 'rgba(52,211,153,0.18)'
      this.autoScrollBtn.style.borderColor = 'rgba(52,211,153,0.6)'
    } else {
      this.autoScrollBtn.textContent = '⏸ Paused'
      this.autoScrollBtn.style.color = '#fbbf24'
      this.autoScrollBtn.style.background = 'rgba(251,191,36,0.18)'
      this.autoScrollBtn.style.borderColor = 'rgba(251,191,36,0.6)'
    }
  }

  private scrollLogToBottom() {
    if (!this.logList) return
    this.logList.scrollTop = this.logList.scrollHeight
  }

  private getEventCategory(eventType?: GameEventType): keyof LogFilters['events'] {
    if (!eventType) return 'system'

    const gameEvents: GameEventType[] = ['GAME_START', 'GAME_END', 'GAME_SPAWNING']
    const nukeEvents: GameEventType[] = ['NUKE_LAUNCHED', 'NUKE_EXPLODED', 'NUKE_INTERCEPTED']
    const alertEvents: GameEventType[] = ['ALERT_NUKE', 'ALERT_HYDRO', 'ALERT_MIRV', 'ALERT_LAND', 'ALERT_NAVAL']
    const troopEvents: GameEventType[] = ['TROOP_UPDATE']
    const systemEvents: GameEventType[] = ['INFO', 'ERROR', 'HARDWARE_TEST']

    if (gameEvents.includes(eventType)) return 'game'
    if (nukeEvents.includes(eventType)) return 'nukes'
    if (alertEvents.includes(eventType)) return 'alerts'
    if (troopEvents.includes(eventType)) return 'troops'
    if (systemEvents.includes(eventType)) return 'system'

    return 'system'
  }

  pushLog(direction: LogDirection, text: string, eventType?: GameEventType) {
    this.ensure()
    if (!this.logList) return

    // Check if this direction is filtered
    if (!this.logFilters.directions[direction]) {
      return
    }

    // Check if this event type is filtered
    const eventCategory = this.getEventCategory(eventType)
    if (!this.logFilters.events[eventCategory]) {
      return
    }

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
    line.setAttribute('data-log-direction', direction)
    line.setAttribute('data-log-category', eventCategory)

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

    // Auto-scroll only if enabled
    if (this.autoScroll) {
      this.scrollLogToBottom()
    }
  }

  private applyFiltersToLog() {
    if (!this.logList) return

    const logLines = this.logList.querySelectorAll('[data-log-direction]')
    logLines.forEach((line) => {
      const direction = line.getAttribute('data-log-direction') as LogDirection
      const category = line.getAttribute('data-log-category') as keyof LogFilters['events']
      const htmlLine = line as HTMLElement

      // Show only if both direction AND event category are enabled
      if (this.logFilters.directions[direction] && this.logFilters.events[category]) {
        htmlLine.style.display = 'flex'
      } else {
        htmlLine.style.display = 'none'
      }
    })
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
    const newWidth = Math.max(250, startWidth + dx)

    container.style.width = `${newWidth}px`
    container.style.height = `${newHeight}px`

    const size: HudSize = { width: newWidth, height: newHeight }
    GM_setValue(STORAGE_KEY_HUD_SIZE, size)
  })

  window.addEventListener('mouseup', () => {
    resizing = false
  })
}
