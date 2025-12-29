import { BaseWindow, FloatingPanel } from './window'

declare function GM_getValue<T = unknown>(key: string, defaultValue?: T): T
declare function GM_setValue<T = unknown>(key: string, value: T): void

const STORAGE_KEY_HUD_POS = 'ots-hud-sidebar-pos'
const STORAGE_KEY_HUD_SIZE = 'ots-hud-size'
const STORAGE_KEY_HUD_SNAP = 'ots-hud-snap-position'
const STORAGE_KEY_HUD_COLLAPSED = 'ots-hud-collapsed'
const STORAGE_KEY_LOG_FILTERS = 'ots-hud-log-filters'

import type { WsStatus, GameEventType } from '../../../ots-shared/src/game'

type SnapPosition = 'top' | 'bottom' | 'left' | 'right'
type HudPos = { left: number; top: number }
type HudSize = { width: number; height: number }
type LogFilters = {
  directions: { send: boolean; recv: boolean; info: boolean }
  events: {
    game: boolean        // GAME_START, GAME_END, GAME_SPAWNING
    nukes: boolean       // NUKE_LAUNCHED, NUKE_EXPLODED, NUKE_INTERCEPTED
    alerts: boolean      // ALERT_NUKE, ALERT_HYDRO, ALERT_MIRV, ALERT_LAND, ALERT_NAVAL
    troops: boolean      // TROOP_UPDATE
    sounds: boolean      // SOUND_PLAY
    system: boolean      // INFO, ERROR, HARDWARE_TEST
    heartbeat: boolean   // Heartbeat messages (INFO with message="heartbeat")
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
  private titlebar: HTMLDivElement | null = null
  private content: HTMLDivElement | null = null
  private closed = false
  private collapsed = false
  private currentPosition: SnapPosition = 'right'
  private isDragging = false
  private dragStartX = 0
  private dragStartY = 0
  private userSize = { width: 320, height: 260 }
  private isResizing = false
  private resizeStartX = 0
  private resizeStartY = 0
  private resizeStartWidth = 0
  private resizeStartHeight = 0

  private logList: HTMLDivElement | null = null
  private wsDot: HTMLSpanElement | null = null
  private gameDot: HTMLSpanElement | null = null
  private body: HTMLDivElement | null = null
  private settingsPanel: HTMLDivElement | null = null
  private settingsInput: HTMLInputElement | null = null
  private filterPanel: HTMLDivElement | null = null
  private filterCheckboxes = {
    directions: { send: null as HTMLInputElement | null, recv: null as HTMLInputElement | null, info: null as HTMLInputElement | null },
    events: { game: null as HTMLInputElement | null, nukes: null as HTMLInputElement | null, alerts: null as HTMLInputElement | null, troops: null as HTMLInputElement | null, sounds: null as HTMLInputElement | null, system: null as HTMLInputElement | null, heartbeat: null as HTMLInputElement | null }
  }
  private logFilters: LogFilters = {
    directions: { send: true, recv: true, info: true },
    events: { game: true, nukes: true, alerts: true, troops: true, sounds: true, system: true, heartbeat: false }
  }
  private logCounter = 0
  private autoScroll = true
  private autoScrollBtn: HTMLButtonElement | null = null
  private activeTab: 'logs' | 'hardware' | 'sound' = 'logs'
  private hardwareDiagnostic: any = null
  private soundToggles: Record<string, boolean> = {
    'game_start': true,
    'game_player_death': true,
    'game_victory': true,
    'game_defeat': true
  }

  constructor(
    private getWsUrl: () => string,
    private setWsUrl: (url: string) => void,
    private onWsUrlChanged: () => void,
    private sendCommand?: (action: string, params?: unknown) => void,
    private onSoundTest?: (soundId: string) => void
  ) {
    // Load saved state
    this.collapsed = GM_getValue<boolean>(STORAGE_KEY_HUD_COLLAPSED, true)
    this.currentPosition = GM_getValue<SnapPosition>(STORAGE_KEY_HUD_SNAP, 'right')
    this.userSize = GM_getValue<HudSize>(STORAGE_KEY_HUD_SIZE, { width: 320, height: 260 })
  }

  ensure() {
    if (this.closed || this.root) return

    this.createHud()
    this.setupEventListeners()
    this.applyPosition(this.currentPosition)
    this.updateContentVisibility()
  }

  private createHud() {
    // Main container
    const root = document.createElement('div')
    root.id = 'ots-userscript-hud'
    root.style.cssText = `
      position: fixed;
      display: flex;
      flex-direction: column;
      background: rgba(20, 20, 30, 0.95);
      border: 2px solid rgba(30,64,175,0.8);
      border-radius: 8px;
      box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);
      z-index: 2147483647;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      transition: width 0.3s cubic-bezier(0.4, 0, 0.2, 1), height 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    `

    // Titlebar
    const titlebar = document.createElement('div')
    titlebar.style.cssText = `
      background: linear-gradient(135deg, #1e293b 0%, #0f172a 100%);
      color: white;
      padding: 4px 6px;
      cursor: move;
      user-select: none;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 4px;
      font-weight: 600;
      font-size: 12px;
      border-radius: 6px 6px 0 0;
      position: relative;
    `

    titlebar.innerHTML = `
      <div style="display:flex;align-items:center;gap:4px;flex-shrink:0;">
        <span style="display:inline-flex;align-items:center;gap:3px;padding:2px 4px;border-radius:10px;background:rgba(0,0,0,0.3);font-size:10px;font-weight:600;letter-spacing:0.05em;color:#e5e7eb;white-space:nowrap;">
          <span id="ots-hud-ws-dot" style="display:inline-flex;width:6px;height:6px;border-radius:999px;background:#f87171;"></span>
          <span>WS</span>
        </span>
        <span style="display:inline-flex;align-items:center;gap:3px;padding:2px 4px;border-radius:10px;background:rgba(0,0,0,0.3);font-size:10px;font-weight:600;letter-spacing:0.05em;color:#e5e7eb;white-space:nowrap;">
          <span id="ots-hud-game-dot" style="display:inline-flex;width:6px;height:6px;border-radius:999px;background:#f87171;"></span>
          <span>GAME</span>
        </span>
      </div>
      <div id="ots-hud-title-content" style="position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);display:flex;align-items:center;gap:4px;white-space:nowrap;">
        <span id="ots-hud-arrow-left" style="display:inline-flex;align-items:center;justify-content:center;font-size:8px;color:#94a3b8;line-height:1;width:10px;height:10px;overflow:hidden;flex-shrink:0;">▶</span>
        <span style="font-size:11px;font-weight:600;letter-spacing:0.02em;color:#cbd5e1;white-space:nowrap;">OTS Dashboard</span>
        <span id="ots-hud-arrow-right" style="display:inline-flex;align-items:center;justify-content:center;font-size:8px;color:#94a3b8;line-height:1;width:10px;height:10px;overflow:hidden;flex-shrink:0;">◀</span>
      </div>
      <div id="ots-hud-buttons" style="display:flex;align-items:center;gap:2px;flex-shrink:0;">
        <button id="ots-hud-settings" style="all:unset;cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:13px;color:white;width:18px;height:18px;border-radius:3px;background:rgba(255,255,255,0.2);transition:background 0.2s;">⚙</button>
        <button id="ots-hud-close" style="all:unset;cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:13px;color:white;width:18px;height:18px;border-radius:3px;background:rgba(255,255,255,0.2);transition:background 0.2s;">×</button>
      </div>
    `

    // Content
    const content = document.createElement('div')
    content.id = 'ots-hud-content'
    content.style.cssText = `
      flex: 1;
      min-height: 0;
      display: flex;
      flex-direction: column;
      overflow: hidden;
    `

    content.innerHTML = `
      <div id="ots-hud-tabs" style="display:flex;gap:2px;padding:6px 8px;background:rgba(10,10,15,0.6);border-bottom:1px solid rgba(255,255,255,0.1);">
        <button class="ots-tab-btn" data-tab="logs" style="all:unset;cursor:pointer;font-size:10px;font-weight:600;color:#e5e7eb;padding:4px 12px;border-radius:4px;background:rgba(59,130,246,0.3);transition:all 0.2s;">Logs</button>
        <button class="ots-tab-btn" data-tab="hardware" style="all:unset;cursor:pointer;font-size:10px;font-weight:600;color:#9ca3af;padding:4px 12px;border-radius:4px;background:rgba(255,255,255,0.05);transition:all 0.2s;">Hardware</button>
        <button class="ots-tab-btn" data-tab="sound" style="all:unset;cursor:pointer;font-size:10px;font-weight:600;color:#9ca3af;padding:4px 12px;border-radius:4px;background:rgba(255,255,255,0.05);transition:all 0.2s;">Sound</button>
      </div>
      <div id="ots-hud-body" style="display:flex;flex-direction:column;height:100%;overflow:hidden;">
        <div id="ots-tab-logs" class="ots-tab-content" style="flex:1;display:flex;flex-direction:column;min-height:0;">
          <div id="ots-hud-log" style="flex:1;overflow-y:auto;overflow-x:hidden;padding:8px;font-size:11px;line-height:1.4;background:rgba(10,10,15,0.8);"></div>
          <div id="ots-hud-footer" style="flex:none;padding:6px 8px;display:flex;align-items:center;justify-content:space-between;background:rgba(10,10,15,0.8);border-top:1px solid rgba(255,255,255,0.1);">
            <button id="ots-hud-filter" style="all:unset;cursor:pointer;font-size:11px;color:#e0e0e0;padding:3px 8px;border-radius:4px;background:rgba(255,255,255,0.1);transition:background 0.2s;">⚑ Filter</button>
            <button id="ots-hud-autoscroll" style="all:unset;cursor:pointer;font-size:10px;font-weight:600;color:#6ee7b7;padding:3px 8px;border-radius:12px;background:rgba(52,211,153,0.2);white-space:nowrap;transition:background 0.2s;">✓ Auto</button>
          </div>
        </div>
        <div id="ots-tab-hardware" class="ots-tab-content" style="flex:1;display:none;overflow-y:auto;padding:12px;background:rgba(10,10,15,0.8);">
          <div id="ots-hardware-content" style="font-size:11px;color:#e5e7eb;"></div>
        </div>
        <div id="ots-tab-sound" class="ots-tab-content" style="flex:1;display:none;overflow-y:auto;padding:12px;background:rgba(10,10,15,0.8);">
          <div style="font-size:11px;color:#e5e7eb;"><div style="font-size:10px;font-weight:600;color:#9ca3af;margin-bottom:8px;letter-spacing:0.05em;">SOUND EVENT TOGGLES</div><div id="ots-sound-toggles"></div></div>
        </div>
      </div>
      <div id="ots-hud-resize-handle" style="position:absolute;width:20px;height:20px;cursor:nwse-resize;display:none;opacity:0.5;transition:opacity 0.2s;">
        <svg width="20" height="20" viewBox="0 0 20 20" style="width:100%;height:100%;">
          <path d="M20,10 L10,20 M20,5 L5,20 M20,0 L0,20" stroke="#94a3b8" stroke-width="2" fill="none"/>
        </svg>
      </div>
    `

    root.appendChild(titlebar)
    root.appendChild(content)
    document.body.appendChild(root)

    // Create settings and filter panels (floating)
    this.createSettingsPanel()
    this.createFilterPanel()

    this.root = root
    this.titlebar = titlebar
    this.content = content
    this.logList = root.querySelector('#ots-hud-log') as HTMLDivElement
    this.body = root.querySelector('#ots-hud-body') as HTMLDivElement
    this.wsDot = root.querySelector('#ots-hud-ws-dot') as HTMLSpanElement
    this.gameDot = root.querySelector('#ots-hud-game-dot') as HTMLSpanElement
    this.autoScrollBtn = root.querySelector('#ots-hud-autoscroll') as HTMLButtonElement

    // Initialize arrow indicators will be set by applyPosition

    // Load saved filters
    const savedFilters = GM_getValue<LogFilters | null>(STORAGE_KEY_LOG_FILTERS, null)
    if (savedFilters) {
      this.logFilters = savedFilters
      this.restoreFilterStates(savedFilters)
    }
  }

  private createSettingsPanel() {
    const panel = document.createElement('div')
    panel.id = 'ots-hud-settings-panel'
    panel.style.cssText = `
      position:fixed;min-width:260px;max-width:320px;display:none;flex-direction:column;gap:6px;padding:8px;border-radius:6px;background:rgba(15,23,42,0.98);border:1px solid rgba(59,130,246,0.6);box-shadow:0 8px 20px rgba(0,0,0,0.5);z-index:2147483647;
    `

    panel.innerHTML = `
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
    `

    document.body.appendChild(panel)
    this.settingsPanel = panel
    this.settingsInput = panel.querySelector('#ots-hud-settings-ws') as HTMLInputElement
  }

  private createFilterPanel() {
    const panel = document.createElement('div')
    panel.id = 'ots-hud-filter-panel'
    panel.style.cssText = `
      position:fixed;min-width:220px;max-width:280px;display:none;flex-direction:column;gap:8px;padding:10px;border-radius:6px;background:rgba(15,23,42,0.98);border:1px solid rgba(59,130,246,0.6);box-shadow:0 8px 20px rgba(0,0,0,0.5);z-index:2147483647;
    `

    panel.innerHTML = `
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
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
          <input type="checkbox" id="ots-hud-filter-sounds" checked style="cursor:pointer;" />
          <span style="color:#06b6d4;">Sound Events</span>
        </label>
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
          <input type="checkbox" id="ots-hud-filter-system" checked style="cursor:pointer;" />
          <span style="color:#94a3b8;">System Events</span>
        </label>
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;">
          <input type="checkbox" id="ots-hud-filter-heartbeat" style="cursor:pointer;" />
          <span style="color:#64748b;">Heartbeat</span>
        </label>
      </div>
    `

    document.body.appendChild(panel)
    this.filterPanel = panel

    this.filterCheckboxes.directions.send = panel.querySelector('#ots-hud-filter-send') as HTMLInputElement
    this.filterCheckboxes.directions.recv = panel.querySelector('#ots-hud-filter-recv') as HTMLInputElement
    this.filterCheckboxes.directions.info = panel.querySelector('#ots-hud-filter-info') as HTMLInputElement
    this.filterCheckboxes.events.game = panel.querySelector('#ots-hud-filter-game') as HTMLInputElement
    this.filterCheckboxes.events.nukes = panel.querySelector('#ots-hud-filter-nukes') as HTMLInputElement
    this.filterCheckboxes.events.alerts = panel.querySelector('#ots-hud-filter-alerts') as HTMLInputElement
    this.filterCheckboxes.events.troops = panel.querySelector('#ots-hud-filter-troops') as HTMLInputElement
    this.filterCheckboxes.events.sounds = panel.querySelector('#ots-hud-filter-sounds') as HTMLInputElement
    this.filterCheckboxes.events.system = panel.querySelector('#ots-hud-filter-system') as HTMLInputElement
    this.filterCheckboxes.events.heartbeat = panel.querySelector('#ots-hud-filter-heartbeat') as HTMLInputElement
  }

  private restoreFilterStates(filters: LogFilters) {
    if (this.filterCheckboxes.directions.send) this.filterCheckboxes.directions.send.checked = filters.directions.send
    if (this.filterCheckboxes.directions.recv) this.filterCheckboxes.directions.recv.checked = filters.directions.recv
    if (this.filterCheckboxes.directions.info) this.filterCheckboxes.directions.info.checked = filters.directions.info
    if (this.filterCheckboxes.events.game) this.filterCheckboxes.events.game.checked = filters.events.game
    if (this.filterCheckboxes.events.nukes) this.filterCheckboxes.events.nukes.checked = filters.events.nukes
    if (this.filterCheckboxes.events.alerts) this.filterCheckboxes.events.alerts.checked = filters.events.alerts
    if (this.filterCheckboxes.events.troops) this.filterCheckboxes.events.troops.checked = filters.events.troops
    if (this.filterCheckboxes.events.sounds) this.filterCheckboxes.events.sounds.checked = filters.events.sounds
    if (this.filterCheckboxes.events.system) this.filterCheckboxes.events.system.checked = filters.events.system
    if (this.filterCheckboxes.events.heartbeat) this.filterCheckboxes.events.heartbeat.checked = filters.events.heartbeat
  }

  private setupEventListeners() {
    if (!this.root || !this.titlebar) return

    // Add hover effects to titlebar buttons
    const titlebarButtons = this.root.querySelectorAll('#ots-hud-buttons button')
    titlebarButtons.forEach((btn) => {
      btn.addEventListener('mouseenter', () => {
        ; (btn as HTMLElement).style.background = 'rgba(255, 255, 255, 0.3)'
      })
      btn.addEventListener('mouseleave', () => {
        ; (btn as HTMLElement).style.background = 'rgba(255, 255, 255, 0.2)'
      })
    })

    // Close HUD
    const closeBtn = this.root.querySelector('#ots-hud-close') as HTMLButtonElement
    closeBtn?.addEventListener('click', () => {
      this.close()
    })

    // Settings button
    const settingsBtn = this.root.querySelector('#ots-hud-settings') as HTMLButtonElement
    settingsBtn?.addEventListener('click', () => {
      this.toggleSettings()
    })

    // Add hover effect to filter button
    const filterBtn = this.root.querySelector('#ots-hud-filter') as HTMLButtonElement
    if (filterBtn) {
      filterBtn.addEventListener('mouseenter', () => {
        filterBtn.style.background = 'rgba(255, 255, 255, 0.15)'
      })
      filterBtn.addEventListener('mouseleave', () => {
        filterBtn.style.background = 'rgba(255, 255, 255, 0.1)'
      })
      filterBtn.addEventListener('click', () => {
        this.toggleFilter()
      })
    }

    // Auto-scroll button
    this.autoScrollBtn?.addEventListener('click', () => {
      this.autoScroll = !this.autoScroll
      this.updateAutoScrollBtn()
    })

    // Titlebar click to toggle collapse/expand
    this.titlebar.addEventListener('click', (e) => {
      // Don't toggle if clicking on buttons
      if ((e.target as HTMLElement).tagName === 'BUTTON' || (e.target as HTMLElement).closest('button')) return
      this.collapsed = !this.collapsed
      GM_setValue(STORAGE_KEY_HUD_COLLAPSED, this.collapsed)
      this.updateContentVisibility()
      this.updateArrows()
    })

    // Dragging
    this.titlebar.addEventListener('mousedown', (e) => {
      // Don't drag if clicking on buttons
      if ((e.target as HTMLElement).tagName === 'BUTTON' || (e.target as HTMLElement).closest('button')) return
      // Mark as potential drag (will be confirmed if mouse moves > threshold)
      this.isDragging = false // Start as false, will be set to true if mouse moves enough
      this.dragStartX = e.clientX
      this.dragStartY = e.clientY
      e.preventDefault()
    })

    // Resize handle events
    const resizeHandle = this.root.querySelector('#ots-hud-resize-handle') as HTMLElement
    if (resizeHandle) {
      resizeHandle.addEventListener('mouseenter', () => {
        resizeHandle.style.opacity = '1'
      })
      resizeHandle.addEventListener('mouseleave', () => {
        if (!this.isResizing) resizeHandle.style.opacity = '0.5'
      })
      resizeHandle.addEventListener('mousedown', (e: MouseEvent) => {
        if (this.collapsed) return
        e.stopPropagation()
        e.preventDefault()
        this.isResizing = true
        this.resizeStartX = e.clientX
        this.resizeStartY = e.clientY
        this.resizeStartWidth = this.userSize.width
        this.resizeStartHeight = this.userSize.height
        resizeHandle.style.opacity = '1'
        // Disable transitions and optimize for resize
        if (this.root) {
          this.root.style.transition = 'none'
          this.root.style.willChange = 'width, height'
        }
      })
    }

    document.addEventListener('mousemove', (e) => {
      // Handle resize
      if (this.isResizing && this.root) {
        e.preventDefault()
        e.stopPropagation()

        const minSize = 200 // Minimum content size
        const minTitlebar = 32
        const pos = this.currentPosition

        if (pos === 'left' || pos === 'right') {
          // Horizontal resize
          const deltaX = pos === 'left' ? (e.clientX - this.resizeStartX) : (this.resizeStartX - e.clientX)
          const newWidth = Math.max(minSize, this.resizeStartWidth + deltaX)

          // Vertical resize
          const deltaY = e.clientY - this.resizeStartY
          const newHeight = Math.max(minSize, this.resizeStartHeight + deltaY)

          // Update stored size
          this.userSize.width = newWidth
          this.userSize.height = newHeight

          // Direct dimension update for smoother resize
          this.root.style.width = `${minTitlebar + newWidth + 2}px`
          this.root.style.height = `${newHeight}px`
        } else {
          // Horizontal resize for top/bottom (centered, so double the delta)
          const deltaX = (e.clientX - this.resizeStartX) * 2
          const newWidth = Math.max(minSize, this.resizeStartWidth + deltaX)

          // Vertical resize
          const deltaY = pos === 'top' ? (e.clientY - this.resizeStartY) : (this.resizeStartY - e.clientY)
          const newHeight = Math.max(minSize, this.resizeStartHeight + deltaY)

          // Update stored size
          this.userSize.width = newWidth
          this.userSize.height = newHeight

          // Direct dimension update for smoother resize
          this.root.style.width = `${newWidth}px`
          this.root.style.height = `${minTitlebar + newHeight + 2}px`
        }
        return
      }

      // Handle drag (check for threshold first)
      if (this.dragStartX !== 0 || this.dragStartY !== 0) {
        const dx = Math.abs(e.clientX - this.dragStartX)
        const dy = Math.abs(e.clientY - this.dragStartY)
        const dragThreshold = 5 // pixels

        // Only activate drag if mouse moved beyond threshold
        if (dx > dragThreshold || dy > dragThreshold) {
          if (!this.isDragging) {
            this.isDragging = true
            this.titlebar!.style.cursor = 'grabbing'
          }
          const snapPos = this.calculateSnapPosition(e.clientX, e.clientY)
          this.showSnapPreview(snapPos)
        }
      }
    })

    document.addEventListener('mouseup', (e) => {
      // Handle resize end
      if (this.isResizing) {
        this.isResizing = false
        GM_setValue(STORAGE_KEY_HUD_SIZE, this.userSize)
        const resizeHandle = this.root?.querySelector('#ots-hud-resize-handle') as HTMLElement
        if (resizeHandle) resizeHandle.style.opacity = '0.5'
        // Re-enable transitions and remove optimization hint
        if (this.root) {
          this.root.style.transition = 'width 0.3s cubic-bezier(0.4, 0, 0.2, 1), height 0.3s cubic-bezier(0.4, 0, 0.2, 1)'
          this.root.style.willChange = 'auto'
        }
        return
      }

      // Handle drag end
      if (this.isDragging) {
        this.isDragging = false
        this.dragStartX = 0
        this.dragStartY = 0
        this.titlebar!.style.cursor = 'move'
        const newPosition = this.calculateSnapPosition(e.clientX, e.clientY)
        this.currentPosition = newPosition
        GM_setValue(STORAGE_KEY_HUD_SNAP, newPosition)
        this.applyPosition(newPosition)
        this.removeSnapPreview()
        return
      }

      // Reset drag tracking if it was just a click
      if (this.dragStartX !== 0 || this.dragStartY !== 0) {
        this.dragStartX = 0
        this.dragStartY = 0
      }
    })

    // Settings panel events
    this.setupSettingsPanelEvents()

    // Tab switching
    const tabBtns = this.root?.querySelectorAll('.ots-tab-btn')
    tabBtns?.forEach(btn => {
      btn.addEventListener('click', () => {
        const tab = (btn as HTMLElement).dataset.tab as 'logs' | 'hardware' | 'sound'
        this.switchTab(tab)
      })
    })

    // Initialize tab content
    this.initializeSoundToggles()
    this.initializeHardwareDisplay()
    // Filter panel events
    this.setupFilterPanelEvents()
  }

  private updateArrows() {
    const arrowLeft = this.root?.querySelector('#ots-hud-arrow-left') as HTMLElement
    const arrowRight = this.root?.querySelector('#ots-hud-arrow-right') as HTMLElement
    if (!arrowLeft || !arrowRight) return

    const collapsed = this.collapsed
    const position = this.currentPosition

    if (position === 'left') {
      // Left edge: outward = left, inward = right
      arrowLeft.textContent = collapsed ? '◀' : '▶'
      arrowRight.textContent = collapsed ? '◀' : '▶'
    } else if (position === 'right') {
      // Right edge: outward = right, inward = left
      arrowLeft.textContent = collapsed ? '▶' : '◀'
      arrowRight.textContent = collapsed ? '▶' : '◀'
    } else if (position === 'top') {
      // Top edge: outward = down, inward = up
      arrowLeft.textContent = collapsed ? '▼' : '▲'
      arrowRight.textContent = collapsed ? '▼' : '▲'
    } else if (position === 'bottom') {
      // Bottom edge: outward = up, inward = down
      arrowLeft.textContent = collapsed ? '▲' : '▼'
      arrowRight.textContent = collapsed ? '▲' : '▼'
    }
  }

  private setupSettingsPanelEvents() {
    if (!this.settingsPanel || !this.settingsInput) return

    const closeBtn = this.settingsPanel.querySelector('#ots-hud-settings-close') as HTMLButtonElement
    closeBtn?.addEventListener('click', () => {
      this.settingsPanel!.style.display = 'none'
    })

    const resetBtn = this.settingsPanel.querySelector('#ots-hud-settings-reset') as HTMLButtonElement
    resetBtn?.addEventListener('click', () => {
      this.settingsInput!.value = 'ws://localhost:3000/ws'
    })

    const saveBtn = this.settingsPanel.querySelector('#ots-hud-settings-save') as HTMLButtonElement
    saveBtn?.addEventListener('click', () => {
      const url = this.settingsInput!.value.trim()
      if (url) {
        this.setWsUrl(url)
        this.logInfo(`WS URL updated to ${url}`)
        this.settingsPanel!.style.display = 'none'
        this.onWsUrlChanged()
      }
    })

    // Make settings panel draggable
    const header = this.settingsPanel.querySelector('#ots-hud-settings-header') as HTMLDivElement
    const settingsFloatingPanel = new FloatingPanel(this.settingsPanel)
    settingsFloatingPanel.attachDrag(header)
  }

  private setupFilterPanelEvents() {
    if (!this.filterPanel) return

    const closeBtn = this.filterPanel.querySelector('#ots-hud-filter-close') as HTMLButtonElement
    closeBtn?.addEventListener('click', () => {
      this.filterPanel!.style.display = 'none'
    })

    // Direction checkboxes
    Object.entries(this.filterCheckboxes.directions).forEach(([key, checkbox]) => {
      checkbox?.addEventListener('change', () => {
        this.logFilters.directions[key as keyof typeof this.logFilters.directions] = checkbox.checked
        GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters)
        this.refilterLogs()
      })
    })

    // Event checkboxes
    Object.entries(this.filterCheckboxes.events).forEach(([key, checkbox]) => {
      checkbox?.addEventListener('change', () => {
        this.logFilters.events[key as keyof typeof this.logFilters.events] = checkbox.checked
        GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters)
        this.refilterLogs()
      })
    })

    // Make filter panel draggable
    const header = this.filterPanel.querySelector('#ots-hud-filter-header') as HTMLDivElement
    const filterFloatingPanel = new FloatingPanel(this.filterPanel)
    filterFloatingPanel.attachDrag(header)
  }

  private calculateSnapPosition(x: number, y: number): SnapPosition {
    const w = window.innerWidth
    const h = window.innerHeight

    const distLeft = x
    const distRight = w - x
    const distTop = y
    const distBottom = h - y

    const minDist = Math.min(distLeft, distRight, distTop, distBottom)

    if (minDist === distLeft) return 'left'
    if (minDist === distRight) return 'right'
    if (minDist === distTop) return 'top'
    return 'bottom'
  }

  private applyPosition(position: SnapPosition) {
    if (!this.root || !this.titlebar) return

    // Reset all positioning
    this.root.style.top = ''
    this.root.style.bottom = ''
    this.root.style.left = ''
    this.root.style.right = ''
    this.root.style.transform = ''
    this.root.style.borderTop = ''
    this.root.style.borderBottom = ''
    this.root.style.borderLeft = ''
    this.root.style.borderRight = ''
    this.titlebar.style.writingMode = ''
    this.titlebar.style.transform = ''
    this.titlebar.style.borderRadius = ''
    this.titlebar.style.width = ''
    this.titlebar.style.height = ''
    this.titlebar.style.flexDirection = ''

    // Get button container for counter-rotation
    const buttonContainer = this.root.querySelector('#ots-hud-buttons') as HTMLElement
    const resizeHandle = this.root.querySelector('#ots-hud-resize-handle') as HTMLElement

    const minSize = 32 // Minimal titlebar size
    const contentWidth = this.collapsed ? 0 : Math.max(200, this.userSize.width)
    const contentHeight = this.collapsed ? 0 : Math.max(150, this.userSize.height)

    switch (position) {
      case 'top':
        this.root.style.top = '-2px'
        this.root.style.left = '50%'
        this.root.style.transform = 'translateX(-50%)'
        this.root.style.width = this.collapsed ? '325px' : `${Math.max(325, contentWidth)}px`
        this.root.style.height = `${minSize + contentHeight + 2}px`
        this.root.style.borderRadius = '0 0 8px 8px'
        this.root.style.borderTop = 'none'
        this.titlebar.style.borderRadius = '0 0 8px 8px'
        // Reset button rotation for horizontal
        if (buttonContainer) buttonContainer.style.transform = ''
        // Position resize handle
        if (resizeHandle) {
          resizeHandle.style.display = this.collapsed ? 'none' : 'block'
          resizeHandle.style.right = '0'
          resizeHandle.style.bottom = '0'
          resizeHandle.style.top = ''
          resizeHandle.style.left = ''
          resizeHandle.style.cursor = 'nwse-resize'
        }
        break

      case 'bottom':
        this.root.style.bottom = '-2px'
        this.root.style.left = '50%'
        this.root.style.transform = 'translateX(-50%)'
        this.root.style.width = this.collapsed ? '325px' : `${Math.max(325, contentWidth)}px`
        this.root.style.height = `${minSize + contentHeight + 2}px`
        this.root.style.borderRadius = '8px 8px 0 0'
        this.root.style.borderBottom = 'none'
        this.titlebar.style.borderRadius = '8px 8px 0 0'
        // Reset button rotation for horizontal
        if (buttonContainer) buttonContainer.style.transform = ''
        // Position resize handle
        if (resizeHandle) {
          resizeHandle.style.display = this.collapsed ? 'none' : 'block'
          resizeHandle.style.right = '0'
          resizeHandle.style.bottom = '0'
          resizeHandle.style.top = ''
          resizeHandle.style.left = ''
          resizeHandle.style.cursor = 'nwse-resize'
        }
        break

      case 'left':
        this.root.style.left = '-2px'
        this.root.style.top = '50%'
        this.root.style.transform = 'translateY(-50%)'
        this.root.style.width = `${minSize + contentWidth + 2}px`
        this.root.style.height = this.collapsed ? '250px' : `${Math.max(250, contentHeight)}px`
        this.root.style.borderRadius = '0 8px 8px 0'
        this.root.style.borderLeft = 'none'

        // Rotate titlebar for vertical orientation
        this.titlebar.style.writingMode = 'vertical-rl'
        this.titlebar.style.transform = 'rotate(180deg)'
        this.titlebar.style.borderRadius = '0 8px 8px 0'
        this.titlebar.style.width = '32px'
        this.titlebar.style.height = '100%'
        // Counter-rotate buttons to keep them horizontal
        if (buttonContainer) buttonContainer.style.transform = 'rotate(-180deg)'
        // Position resize handle
        if (resizeHandle) {
          resizeHandle.style.display = this.collapsed ? 'none' : 'block'
          resizeHandle.style.right = '0'
          resizeHandle.style.bottom = '0'
          resizeHandle.style.top = ''
          resizeHandle.style.left = ''
          resizeHandle.style.cursor = 'nwse-resize'
        }
        break

      case 'right':
        this.root.style.right = '-2px'
        this.root.style.top = '50%'
        this.root.style.transform = 'translateY(-50%)'
        this.root.style.width = `${minSize + contentWidth + 2}px`
        this.root.style.height = this.collapsed ? '250px' : `${Math.max(250, contentHeight)}px`
        this.root.style.borderRadius = '8px 0 0 8px'
        this.root.style.borderRight = 'none'

        // Rotate titlebar for vertical orientation
        this.titlebar.style.writingMode = 'vertical-rl'
        this.titlebar.style.transform = 'rotate(180deg)'
        this.titlebar.style.borderRadius = '8px 0 0 8px'
        this.titlebar.style.width = '32px'
        this.titlebar.style.height = '100%'
        // Counter-rotate buttons to keep them horizontal
        if (buttonContainer) buttonContainer.style.transform = 'rotate(-180deg)'
        // Position resize handle
        if (resizeHandle) {
          resizeHandle.style.display = this.collapsed ? 'none' : 'block'
          resizeHandle.style.right = '0'
          resizeHandle.style.bottom = '0'
          resizeHandle.style.top = ''
          resizeHandle.style.left = ''
          resizeHandle.style.cursor = 'nwse-resize'
        }
        break
    }

    // Adjust layout based on orientation
    if (position === 'left' || position === 'right') {
      this.root.style.flexDirection = 'row'
      if (this.content) {
        this.content.style.flex = '1'
        this.content.style.minWidth = '0'
      }
      if (this.body) {
        this.body.style.height = '100%'
      }
    } else {
      this.root.style.flexDirection = 'column'
      if (this.content) {
        this.content.style.flex = '1'
        this.content.style.minHeight = '0'
      }
      if (this.body) {
        this.body.style.height = ''
        this.body.style.flex = '1'
      }
    }

    // Update arrows based on position
    this.updateArrows()
  }

  private showSnapPreview(position: SnapPosition) {
    let preview = document.getElementById('ots-snap-preview')
    if (!preview) {
      preview = document.createElement('div')
      preview.id = 'ots-snap-preview'
      preview.style.cssText = `
        position: fixed;
        background: rgba(59, 130, 246, 0.25);
        border: 2px dashed rgba(59, 130, 246, 0.6);
        z-index: 2147483646;
        pointer-events: none;
        transition: all 0.2s;
      `
      document.body.appendChild(preview)
    }

    const previewSize = 100

    switch (position) {
      case 'top':
        preview.style.top = '0'
        preview.style.left = '50%'
        preview.style.bottom = ''
        preview.style.right = ''
        preview.style.transform = 'translateX(-50%)'
        preview.style.width = '300px'
        preview.style.height = `${previewSize}px`
        break
      case 'bottom':
        preview.style.bottom = '0'
        preview.style.left = '50%'
        preview.style.top = ''
        preview.style.right = ''
        preview.style.transform = 'translateX(-50%)'
        preview.style.width = '300px'
        preview.style.height = `${previewSize}px`
        break
      case 'left':
        preview.style.left = '0'
        preview.style.top = '50%'
        preview.style.bottom = ''
        preview.style.right = ''
        preview.style.transform = 'translateY(-50%)'
        preview.style.width = `${previewSize}px`
        preview.style.height = '300px'
        break
      case 'right':
        preview.style.right = '0'
        preview.style.top = '50%'
        preview.style.left = ''
        preview.style.bottom = ''
        preview.style.transform = 'translateY(-50%)'
        preview.style.width = `${previewSize}px`
        preview.style.height = '300px'
        break
    }
  }

  private removeSnapPreview() {
    const preview = document.getElementById('ots-snap-preview')
    if (preview) {
      preview.remove()
    }
  }

  private updateContentVisibility() {
    if (!this.content) return

    if (this.collapsed) {
      this.content.style.display = 'none'
    } else {
      this.content.style.display = 'flex'
    }

    this.applyPosition(this.currentPosition)
  }

  private positionPanelNextToHud(panel: HTMLDivElement) {
    if (!this.root || !panel) return

    const hudRect = this.root.getBoundingClientRect()
    const gap = 8

    // Clear all position properties
    panel.style.top = ''
    panel.style.bottom = ''
    panel.style.left = ''
    panel.style.right = ''

    // Position based on HUD location
    switch (this.currentPosition) {
      case 'right':
        // HUD on right edge, panel goes to the left
        panel.style.top = `${hudRect.top}px`
        panel.style.right = `${window.innerWidth - hudRect.left + gap}px`
        break
      case 'left':
        // HUD on left edge, panel goes to the right
        panel.style.top = `${hudRect.top}px`
        panel.style.left = `${hudRect.right + gap}px`
        break
      case 'top':
        // HUD on top edge, panel goes below
        panel.style.top = `${hudRect.bottom + gap}px`
        panel.style.left = `${hudRect.left}px`
        break
      case 'bottom':
        // HUD on bottom edge, panel goes above
        panel.style.bottom = `${window.innerHeight - hudRect.top + gap}px`
        panel.style.left = `${hudRect.left}px`
        break
    }
  }

  private requestDiagnostic() {
    if (this.sendCommand) {
      this.sendCommand('hardware-diagnostic', {})
      this.pushLog('info', 'Hardware diagnostic requested')
    } else {
      this.pushLog('info', 'Cannot send diagnostic request: no WebSocket connection')
    }
  }

  private toggleSettings() {
    if (!this.settingsPanel || !this.settingsInput || !this.root) return

    if (this.settingsPanel.style.display === 'none' || this.settingsPanel.style.display === '') {
      this.settingsInput.value = this.getWsUrl()
      this.positionPanelNextToHud(this.settingsPanel)
      this.settingsPanel.style.display = 'flex'
      // Hide filter panel if open
      if (this.filterPanel) this.filterPanel.style.display = 'none'
    } else {
      this.settingsPanel.style.display = 'none'
    }
  }

  private toggleFilter() {
    if (!this.filterPanel || !this.root) return

    if (this.filterPanel.style.display === 'none' || this.filterPanel.style.display === '') {
      this.positionPanelNextToHud(this.filterPanel)
      this.filterPanel.style.display = 'flex'
      // Hide settings panel if open
      if (this.settingsPanel) this.settingsPanel.style.display = 'none'
    } else {
      this.filterPanel.style.display = 'none'
    }
  }

  private updateAutoScrollBtn() {
    if (!this.autoScrollBtn) return

    if (this.autoScroll) {
      this.autoScrollBtn.style.color = '#6ee7b7'
      this.autoScrollBtn.style.borderColor = 'rgba(52,211,153,0.6)'
      this.autoScrollBtn.style.background = 'rgba(52,211,153,0.18)'
      this.autoScrollBtn.textContent = '✓ Auto'
    } else {
      this.autoScrollBtn.style.color = '#9ca3af'
      this.autoScrollBtn.style.borderColor = 'rgba(148,163,184,0.6)'
      this.autoScrollBtn.style.background = 'rgba(15,23,42,0.9)'
      this.autoScrollBtn.textContent = '✗ Auto'
    }
  }

  private refilterLogs() {
    if (!this.logList) return
    const entries = Array.from(this.logList.children) as HTMLDivElement[]
    entries.forEach(entry => {
      const shouldShow = this.shouldShowLogEntry(
        entry.dataset.direction as LogDirection,
        entry.dataset.eventType as GameEventType | undefined,
        entry.dataset.messageText
      )
      entry.style.display = shouldShow ? 'block' : 'none'
    })
  }

  private shouldShowLogEntry(direction: LogDirection, eventType?: GameEventType, messageText?: string): boolean {
    // Check direction filter
    if (!this.logFilters.directions[direction]) return false

    // If no event type, show (it's a raw message)
    if (!eventType) return true

    // Check for heartbeat specifically (INFO event with message containing 'heartbeat')
    if (eventType === 'INFO' && messageText && messageText.includes('"heartbeat"')) {
      return this.logFilters.events.heartbeat
    }

    // Check event type filter
    if (eventType.startsWith('GAME_') || eventType === 'GAME_SPAWNING') {
      return this.logFilters.events.game
    } else if (eventType.includes('NUKE') || eventType.includes('HYDRO') || eventType.includes('MIRV')) {
      return this.logFilters.events.nukes
    } else if (eventType.startsWith('ALERT_')) {
      return this.logFilters.events.alerts
    } else if (eventType.includes('TROOP')) {
      return this.logFilters.events.troops
    } else if (eventType === 'SOUND_PLAY') {
      return this.logFilters.events.sounds
    } else if (eventType === 'INFO' || eventType === 'ERROR' || eventType === 'HARDWARE_TEST') {
      return this.logFilters.events.system
    }

    return true
  }

  private switchTab(tab: 'logs' | 'hardware' | 'sound') {
    this.activeTab = tab

    // Update tab buttons
    const tabBtns = this.root?.querySelectorAll('.ots-tab-btn')
    tabBtns?.forEach(btn => {
      const btnTab = (btn as HTMLElement).dataset.tab
      if (btnTab === tab) {
        ; (btn as HTMLElement).style.background = 'rgba(59,130,246,0.3)'
          ; (btn as HTMLElement).style.color = '#e5e7eb'
      } else {
        ; (btn as HTMLElement).style.background = 'rgba(255,255,255,0.05)'
          ; (btn as HTMLElement).style.color = '#9ca3af'
      }
    })

    // Show/hide content
    const tabContents = this.root?.querySelectorAll('.ots-tab-content')
    tabContents?.forEach(content => {
      ; (content as HTMLElement).style.display = 'none'
    })

    const activeContent = this.root?.querySelector(`#ots-tab-${tab}`) as HTMLElement
    if (activeContent) {
      activeContent.style.display = 'flex'
    }

    // Refresh hardware display if switching to hardware tab
    if (tab === 'hardware') {
      this.updateHardwareDisplay()
    }
  }

  private initializeSoundToggles() {
    const container = this.root?.querySelector('#ots-sound-toggles')
    if (!container) return

    const soundIds = ['game_start', 'game_player_death', 'game_victory', 'game_defeat']
    const soundLabels: Record<string, string> = {
      'game_start': 'Game Start',
      'game_player_death': 'Player Death',
      'game_victory': 'Victory',
      'game_defeat': 'Defeat'
    }

    container.innerHTML = soundIds.map(id => `
      <div style=\"display:flex;align-items:center;gap:8px;margin-bottom:6px;padding:6px 8px;border-radius:4px;background:rgba(255,255,255,0.05);transition:background 0.2s;\" data-sound-row=\"${id}\">
        <label style=\"display:flex;align-items:center;gap:8px;font-size:11px;color:#e5e7eb;cursor:pointer;flex:1;\">
          <input type=\"checkbox\" data-sound-toggle=\"${id}\" ${this.soundToggles[id] ? 'checked' : ''} style=\"cursor:pointer;\" />
          <span style=\"flex:1;\">${soundLabels[id]}</span>
          <code style=\"font-size:9px;color:#94a3b8;font-family:monospace;\">${id}</code>
        </label>
        <button data-sound-test=\"${id}\" style=\"all:unset;cursor:pointer;font-size:11px;padding:4px 10px;border-radius:4px;background:rgba(59,130,246,0.3);color:#93c5fd;font-weight:600;transition:background 0.2s;white-space:nowrap;\">\u25b6 Test</button>
      </div>
    `).join('')

    // Add event listeners for toggles
    container.querySelectorAll('[data-sound-toggle]').forEach(checkbox => {
      checkbox.addEventListener('change', (e) => {
        const soundId = (e.target as HTMLInputElement).dataset.soundToggle!
        this.soundToggles[soundId] = (e.target as HTMLInputElement).checked
        this.logInfo(`Sound ${soundId}: ${this.soundToggles[soundId] ? 'enabled' : 'disabled'}`)
      })
    })

    // Add event listeners for test buttons
    container.querySelectorAll('[data-sound-test]').forEach(button => {
      button.addEventListener('click', (e) => {
        const soundId = (button as HTMLElement).dataset.soundTest!
        this.testSound(soundId)
      })

      // Add hover effect for test button
      button.addEventListener('mouseenter', () => {
        (button as HTMLElement).style.background = 'rgba(59,130,246,0.5)'
      })
      button.addEventListener('mouseleave', () => {
        (button as HTMLElement).style.background = 'rgba(59,130,246,0.3)'
      })
    })

    // Add hover effects for rows
    container.querySelectorAll('[data-sound-row]').forEach(row => {
      row.addEventListener('mouseenter', () => {
        ; (row as HTMLElement).style.background = 'rgba(255,255,255,0.08)'
      })
      row.addEventListener('mouseleave', () => {
        ; (row as HTMLElement).style.background = 'rgba(255,255,255,0.05)'
      })
    })
  }

  private testSound(soundId: string) {
    this.logInfo(`Testing sound: ${soundId}`)
    if (this.onSoundTest) {
      this.onSoundTest(soundId)
    }
  }

  private initializeHardwareDisplay() {
    const container = this.root?.querySelector('#ots-hardware-content')
    if (!container) return

    container.innerHTML = `
      <div style=\"margin-bottom:16px;padding:10px;background:rgba(59,130,246,0.1);border-left:3px solid #3b82f6;border-radius:4px;\">
        <div style=\"font-size:11px;font-weight:600;color:#93c5fd;margin-bottom:8px;\">\u2699\ufe0f DEVICE CONNECTION</div>
        <label style=\"display:flex;flex-direction:column;gap:4px;font-size:10px;color:#e5e7eb;\">
          <span style=\"font-size:9px;font-weight:600;color:#9ca3af;letter-spacing:0.05em;\">WEBSOCKET URL</span>
          <input id=\"ots-device-ws-url\" type=\"text\" placeholder=\"ws://ots-fw-main.local:3000/ws\" value=\"${this.getWsUrl()}\" style=\"font-size:10px;padding:6px 8px;border-radius:4px;border:1px solid rgba(148,163,184,0.4);background:rgba(15,23,42,0.7);color:#e5e7eb;outline:none;font-family:monospace;\" />
          <span style=\"font-size:8px;color:#64748b;\">Connect to firmware device or ots-server</span>
        </label>
      </div>
      <div style=\"margin-bottom:12px;\">
        <button id=\"ots-refresh-device-info\" style=\"all:unset;cursor:pointer;font-size:11px;color:#0f172a;padding:6px 12px;border-radius:4px;background:#4ade80;font-weight:600;width:100%;text-align:center;margin-bottom:12px;\">\ud83d\udd27 Request Hardware Diagnostic</button>
      </div>
      <div style=\"text-align:center;color:#64748b;padding:40px 20px;\">
        <div style=\"font-size:32px;margin-bottom:12px;\">\ud83d\udd27</div>
        <div style=\"font-size:12px;margin-bottom:8px;\">No diagnostic data available</div>
        <div style=\"font-size:10px;\">Click the button above to request hardware diagnostic</div>
      </div>
    `

    // Device WS URL change handler
    const wsUrlInput = container.querySelector('#ots-device-ws-url') as HTMLInputElement
    wsUrlInput?.addEventListener('change', () => {
      const newUrl = wsUrlInput.value.trim()
      if (newUrl) {
        this.setWsUrl(newUrl)
        this.onWsUrlChanged()
        this.logInfo(`Device WS URL updated to ${newUrl}`)
      }
    })

    // Refresh device info button
    const refreshBtn = container.querySelector('#ots-refresh-device-info') as HTMLButtonElement
    refreshBtn?.addEventListener('click', () => {
      this.requestDiagnostic()
    })
  }

  private updateHardwareDisplay() {
    const container = this.root?.querySelector('#ots-hardware-content')
    if (!container) return

    if (!this.hardwareDiagnostic) {
      this.initializeHardwareDisplay()
      return
    }

    const data = this.hardwareDiagnostic
    const hardware = data.hardware || {}

    const componentIcons: Record<string, string> = {
      lcd: '\ud83d\udcfa',
      inputBoard: '\ud83c\udfae',
      outputBoard: '\ud83d\udca1',
      adc: '\ud83d\udcca',
      soundModule: '\ud83d\udd0a'
    }

    const componentLabels: Record<string, string> = {
      lcd: 'LCD Display',
      inputBoard: 'Input Board',
      outputBoard: 'Output Board',
      adc: 'ADC',
      soundModule: 'Sound Module'
    }

    const getStatusIcon = (component: any) => {
      if (!component) return '\u2753'
      if (component.present && component.working) return '\u2705'
      if (component.present && !component.working) return '\u26a0\ufe0f'
      return '\u274c'
    }

    const getStatusColor = (component: any) => {
      if (!component) return '#64748b'
      if (component.present && component.working) return '#4ade80'
      if (component.present && !component.working) return '#facc15'
      return '#f87171'
    }

    const getStatusText = (component: any) => {
      if (!component) return 'Unknown'
      if (component.present && component.working) return 'Working'
      if (component.present && !component.working) return 'Present (Not Working)'
      return 'Not Present'
    }

    container.innerHTML = `
      <div style=\"margin-bottom:16px;padding:10px;background:rgba(59,130,246,0.1);border-left:3px solid #3b82f6;border-radius:4px;\">
        <div style=\"font-size:12px;font-weight:600;color:#93c5fd;margin-bottom:6px;\">\ud83d\udce1 ${data.deviceType || 'Unknown'}</div>
        <div style=\"font-size:10px;color:#94a3b8;line-height:1.6;font-family:monospace;\">
          <div>Serial: <span style=\"color:#e5e7eb;\">${data.serialNumber || 'Unknown'}</span></div>
          <div>Owner: <span style=\"color:#e5e7eb;\">${data.owner || 'Unknown'}</span></div>
          <div>Version: <span style=\"color:#e5e7eb;\">${data.version || 'Unknown'}</span></div>
        </div>
      </div>
      <div style=\"font-size:10px;font-weight:600;color:#9ca3af;margin-bottom:8px;letter-spacing:0.05em;\">HARDWARE COMPONENTS</div>
      ${Object.keys(hardware).map(key => {
      const component = hardware[key]
      return `
          <div style=\"display:flex;align-items:center;gap:10px;margin-bottom:8px;padding:8px;background:rgba(255,255,255,0.03);border-radius:4px;border-left:3px solid ${getStatusColor(component)};\">
            <div style=\"font-size:18px;flex-shrink:0;\">${componentIcons[key] || '\ud83d\udce6'}</div>
            <div style=\"flex:1;\">
              <div style=\"font-size:11px;font-weight:600;color:#e5e7eb;margin-bottom:2px;\">${componentLabels[key] || key}</div>
              <div style=\"font-size:9px;color:#94a3b8;\">${getStatusText(component)}</div>
            </div>
            <div style=\"font-size:16px;flex-shrink:0;\">${getStatusIcon(component)}</div>
          </div>
        `
    }).join('')}
      <div style=\"margin-top:12px;font-size:9px;color:#64748b;text-align:center;\">
        Last updated: ${new Date(data.timestamp || Date.now()).toLocaleTimeString()}
      </div>
    `
  }

  isSoundEnabled(soundId: string): boolean {
    return this.soundToggles[soundId] !== false
  }

  close() {
    this.closed = true
    if (this.root) {
      this.root.style.display = 'none'
    }
    if (this.settingsPanel) {
      this.settingsPanel.style.display = 'none'
    }
    if (this.filterPanel) {
      this.filterPanel.style.display = 'none'
    }
  }

  setWsStatus(status: WsStatus) {
    if (!this.wsDot) return

    switch (status) {
      case 'CONNECTING':
        this.wsDot.style.background = '#facc15'
        break
      case 'OPEN':
        this.wsDot.style.background = '#4ade80'
        break
      case 'DISCONNECTED':
      case 'ERROR':
        this.wsDot.style.background = '#f97373'
        break
    }
  }

  setGameStatus(connected: boolean) {
    if (!this.gameDot) return
    this.gameDot.style.background = connected ? '#4ade80' : '#f97373'
  }

  logSend(text: string) {
    this.addLog('send', text)
  }

  logRecv(text: string, eventType?: GameEventType) {
    // Capture hardware diagnostic data
    if (eventType === 'HARDWARE_DIAGNOSTIC') {
      try {
        const parsed = JSON.parse(text)
        console.log('[OTS HUD] Received HARDWARE_DIAGNOSTIC:', parsed)

        // The message structure is {type: 'event', payload: {type: 'HARDWARE_DIAGNOSTIC', data: {...}}}
        const data = parsed?.payload?.data || parsed?.data
        if (data) {
          this.hardwareDiagnostic = {
            ...data,
            timestamp: parsed?.payload?.timestamp || parsed?.timestamp || Date.now()
          }
          console.log('[OTS HUD] Hardware diagnostic captured:', this.hardwareDiagnostic)
          this.logInfo('Hardware diagnostic data captured - switch to Hardware tab to view')
          // If currently on hardware tab, update the display
          if (this.activeTab === 'hardware') {
            this.updateHardwareDisplay()
          }
        } else {
          console.warn('[OTS HUD] No data found in HARDWARE_DIAGNOSTIC message:', parsed)
        }
      } catch (e) {
        console.error('[OTS HUD] Failed to parse HARDWARE_DIAGNOSTIC:', e, text)
      }
    }
    this.addLog('recv', text, eventType)
  }

  logInfo(text: string) {
    this.addLog('info', text)
  }

  // Alias for compatibility with WsClient
  pushLog(direction: LogDirection, text: string, eventType?: GameEventType) {
    // Capture hardware diagnostic data (same logic as logRecv)
    if (direction === 'recv' && eventType === 'HARDWARE_DIAGNOSTIC') {
      try {
        const parsed = JSON.parse(text)
        console.log('[OTS HUD] Received HARDWARE_DIAGNOSTIC:', parsed)

        const data = parsed?.payload?.data || parsed?.data
        if (data) {
          this.hardwareDiagnostic = {
            ...data,
            timestamp: parsed?.payload?.timestamp || parsed?.timestamp || Date.now()
          }
          console.log('[OTS HUD] Hardware diagnostic captured:', this.hardwareDiagnostic)
          this.logInfo('Hardware diagnostic data captured - switch to Hardware tab to view')
          if (this.activeTab === 'hardware') {
            this.updateHardwareDisplay()
          }
        } else {
          console.warn('[OTS HUD] No data found in HARDWARE_DIAGNOSTIC message:', parsed)
        }
      } catch (e) {
        console.error('[OTS HUD] Failed to parse HARDWARE_DIAGNOSTIC:', e, text)
      }
    }
    this.addLog(direction, text, eventType)
  }

  private addLog(direction: LogDirection, text: string, eventType?: GameEventType) {
    if (!this.logList) return

    // Check if this log should be shown based on filters
    if (!this.shouldShowLogEntry(direction, eventType, text)) return

    const id = ++this.logCounter
    const ts = Date.now()

    const entry = document.createElement('div')
    entry.dataset.id = String(id)
    entry.dataset.direction = direction
    entry.dataset.messageText = text
    if (eventType) entry.dataset.eventType = eventType
    entry.style.cssText = 'margin-bottom:3px;padding:2px 4px;border-radius:3px;background:rgba(15,23,42,0.7);border-left:2px solid;word-break:break-word;'

    const time = new Date(ts).toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' })

    let badge = ''
    let borderColor = ''

    switch (direction) {
      case 'send':
        badge = '<span style="display:inline-flex;font-size:9px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;padding:1px 4px;border-radius:999px;background:rgba(52,211,153,0.18);color:#6ee7b7;margin-right:4px;">SEND</span>'
        borderColor = '#6ee7b7'
        break
      case 'recv':
        badge = '<span style="display:inline-flex;font-size:9px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;padding:1px 4px;border-radius:999px;background:rgba(59,130,246,0.22);color:#93c5fd;margin-right:4px;">RECV</span>'
        borderColor = '#93c5fd'
        break
      case 'info':
        badge = '<span style="display:inline-flex;font-size:9px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;padding:1px 4px;border-radius:999px;background:rgba(156,163,175,0.25);color:#e5e7eb;margin-right:4px;">INFO</span>'
        borderColor = '#9ca3af'
        break
    }

    entry.style.borderLeftColor = borderColor

    // Try to parse as JSON for collapsible display
    let jsonData: any = null
    let summary = text
    try {
      jsonData = JSON.parse(text)
      // Create a summary from the JSON
      if (jsonData.type === 'event' && jsonData.payload) {
        const eventType = jsonData.payload.type || 'Unknown'
        const message = jsonData.payload.message
        summary = `Event: ${eventType}${message ? ' - ' + message : ''}`
      } else if (jsonData.type === 'cmd' && jsonData.payload) {
        const action = jsonData.payload.action || 'Unknown'
        summary = `Command: ${action}`
      } else if (jsonData.type) {
        summary = `${jsonData.type}`
      } else {
        summary = 'JSON Message (click to expand)'
      }
    } catch {
      // Not JSON, use as-is
    }

    if (jsonData) {
      // Collapsible JSON view
      entry.innerHTML = `
        <div style="display:flex;align-items:center;gap:4px;margin-bottom:1px;">
          <span style="font-size:9px;color:#6b7280;font-family:monospace;">${time}</span>${badge}
        </div>
        <div class="log-summary" style="font-size:11px;color:#e5e7eb;line-height:1.3;cursor:pointer;user-select:none;display:flex;align-items:center;gap:4px;">
          <span style="color:#9ca3af;">▶</span>
          <span>${this.escapeHtml(summary)}</span>
        </div>
        <div class="log-details" style="display:none;margin-top:4px;padding:8px;background:rgba(0,0,0,0.4);border-radius:4px;font-family:monospace;font-size:10px;line-height:1.5;overflow-x:auto;">
          ${this.formatJson(jsonData)}
        </div>
      `

      // Toggle expand/collapse
      const summaryEl = entry.querySelector('.log-summary') as HTMLElement
      const detailsEl = entry.querySelector('.log-details') as HTMLElement
      const arrow = summaryEl?.querySelector('span:first-child')

      summaryEl?.addEventListener('click', () => {
        const isExpanded = detailsEl.style.display !== 'none'
        detailsEl.style.display = isExpanded ? 'none' : 'block'
        if (arrow) arrow.textContent = isExpanded ? '▶' : '▼'
      })
    } else {
      // Plain text view
      entry.innerHTML = `
        <div style="display:flex;align-items:center;gap:4px;margin-bottom:1px;">
          <span style="font-size:9px;color:#6b7280;font-family:monospace;">${time}</span>${badge}
        </div>
        <div style="font-size:11px;color:#e5e7eb;line-height:1.3;">${this.escapeHtml(text)}</div>
      `
    }

    this.logList.appendChild(entry)

    // Auto-scroll
    if (this.autoScroll) {
      this.logList.scrollTop = this.logList.scrollHeight
    }

    // Limit to 100 entries
    while (this.logList.children.length > 100) {
      this.logList.removeChild(this.logList.firstChild!)
    }
  }

  private formatJson(obj: any, indent = 0): string {
    const indentStr = '  '.repeat(indent)
    const nextIndent = '  '.repeat(indent + 1)

    if (obj === null) return '<span style="color:#f87171;">null</span>'
    if (obj === undefined) return '<span style="color:#f87171;">undefined</span>'
    if (typeof obj === 'boolean') return `<span style="color:#c084fc;">${obj}</span>`
    if (typeof obj === 'number') return `<span style="color:#facc15;">${obj}</span>`
    if (typeof obj === 'string') return `<span style="color:#4ade80;">"${this.escapeHtml(obj)}"</span>`

    if (Array.isArray(obj)) {
      if (obj.length === 0) return '<span style="color:#94a3b8;">[]</span>'
      const items = obj.map(item => `${nextIndent}${this.formatJson(item, indent + 1)}`).join(',\n')
      return `<span style="color:#94a3b8;">[</span>\n${items}\n${indentStr}<span style="color:#94a3b8;">]</span>`
    }

    if (typeof obj === 'object') {
      const keys = Object.keys(obj)
      if (keys.length === 0) return '<span style="color:#94a3b8;">{}</span>'
      const items = keys.map(key => {
        const value = this.formatJson(obj[key], indent + 1)
        return `${nextIndent}<span style="color:#60a5fa;">"${this.escapeHtml(key)}"</span><span style="color:#94a3b8;">:</span> ${value}`
      }).join(',\n')
      return `<span style="color:#94a3b8;">{</span>\n${items}\n${indentStr}<span style="color:#94a3b8;">}</span>`
    }

    return String(obj)
  }

  private escapeHtml(text: string): string {
    const div = document.createElement('div')
    div.textContent = text
    return div.innerHTML
  }
}
