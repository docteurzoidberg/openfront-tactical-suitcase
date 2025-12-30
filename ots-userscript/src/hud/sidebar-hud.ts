import {
  FloatingPanel,
  PositionManager,
  SettingsPanel,
  FilterPanel,
  HudTemplate,
  TabManager,
  LogsTab,
  HardwareTab,
  SoundTab,
  tryCaptureHardwareDiagnostic,
  DEFAULT_LOG_FILTERS,
  DEFAULT_SOUND_TOGGLES,
  type HudPos,
  type HudSize,
  type LogDirection,
  type LogFilters,
  type SnapPosition,
  type TabId,
  type CapturedHardwareDiagnostic
} from './sidebar'

export type { LogDirection } from './sidebar'

import { STORAGE_KEYS } from '../storage/keys'

import type { WsStatus, GameEventType } from '../../../ots-shared/src/game'

export type { WsStatus }

export class Hud {
  private root: HTMLDivElement | null = null
  private titlebar: HTMLDivElement | null = null
  private content: HTMLDivElement | null = null
  private closed = false
  private collapsed = false
  private positionManager: PositionManager | null = null

  private wsDot: HTMLSpanElement | null = null
  private gameDot: HTMLSpanElement | null = null
  private body: HTMLDivElement | null = null
  private settingsPanel: SettingsPanel | null = null
  private filterPanel: FilterPanel | null = null
  private logFilters: LogFilters = DEFAULT_LOG_FILTERS

  // Tab components
  private tabManager: TabManager | null = null
  private logsTab: LogsTab | null = null
  private hardwareTab: HardwareTab | null = null
  private soundTab: SoundTab | null = null
  private hardwareDiagnostic: CapturedHardwareDiagnostic | null = null
  private soundToggles: Record<string, boolean> = DEFAULT_SOUND_TOGGLES

  constructor(
    private getWsUrl: () => string,
    private setWsUrl: (url: string) => void,
    private onWsUrlChanged: () => void,
    private sendCommand?: (action: string, params?: unknown) => void,
    private onSoundTest?: (soundId: string) => void
  ) {
    // Load saved state
    this.collapsed = GM_getValue<boolean>(STORAGE_KEYS.HUD_COLLAPSED, true)
  }

  ensure() {
    if (this.closed || this.root) return

    this.createHud()
    this.setupEventListeners()

    // Initialize position manager after DOM is ready
    if (this.root && this.titlebar && this.positionManager) {
      this.positionManager.setContainers(this.content, this.body)
      this.positionManager.initialize(this.collapsed)
      this.positionManager.updateArrows(this.collapsed)
    }

    this.updateContentVisibility()
  }

  private createHud() {
    // Create root container using HudTemplate
    const root = HudTemplate.createRoot()

    // Create titlebar using HudTemplate
    const titlebar = HudTemplate.createTitlebar()

    // Create content container using HudTemplate
    const content = HudTemplate.createContent()

    // Create tab manager
    this.tabManager = new TabManager({
      container: content,
      tabs: [
        { id: 'logs', label: 'Logs', contentId: 'ots-tab-logs' },
        { id: 'hardware', label: 'Hardware', contentId: 'ots-tab-hardware' },
        { id: 'sound', label: 'Sound', contentId: 'ots-tab-sound' }
      ],
      defaultTab: 'logs',
      onTabChange: (tabId) => this.handleTabChange(tabId)
    })

    // Generate content HTML using HudTemplate
    content.innerHTML = HudTemplate.createContentHTML(this.tabManager)

    root.appendChild(titlebar)
    root.appendChild(content)
    document.body.appendChild(root)

    // Load saved filters first
    const savedFilters = GM_getValue<LogFilters | null>(STORAGE_KEYS.LOG_FILTERS, null)
    if (savedFilters) {
      this.logFilters = savedFilters
    }

    // Create settings panel component
    this.settingsPanel = new SettingsPanel(
      () => { }, // onClose callback (no action needed)
      (url: string) => {
        this.setWsUrl(url)
        this.logInfo(`WS URL updated to ${url}`)
        this.onWsUrlChanged()
      }
    )

    // Create filter panel component
    this.filterPanel = new FilterPanel(
      () => { }, // onClose callback (no action needed)
      (filters: LogFilters) => {
        this.logFilters = filters
        GM_setValue(STORAGE_KEYS.LOG_FILTERS, this.logFilters)
        this.logsTab?.updateFilters(this.logFilters)
        this.refilterLogs()
      },
      this.logFilters
    )

    this.root = root
    this.titlebar = titlebar
    this.content = content
    this.body = root.querySelector('#ots-hud-body') as HTMLDivElement
    this.wsDot = root.querySelector('#ots-hud-ws-dot') as HTMLSpanElement
    this.gameDot = root.querySelector('#ots-hud-game-dot') as HTMLSpanElement

    // Initialize LogsTab component
    this.logsTab = new LogsTab(
      () => this.toggleFilter(),
      this.logFilters
    )

    // Load position state
    const savedPosition = GM_getValue<SnapPosition>(STORAGE_KEYS.HUD_SNAP, 'right')
    const savedSize = GM_getValue<HudSize>(STORAGE_KEYS.HUD_SIZE, { width: 320, height: 260 })

    // Initialize position manager
    this.positionManager = new PositionManager(
      root,
      titlebar,
      savedPosition,
      savedSize,
      {
        onPositionChange: (position) => {
          GM_setValue(STORAGE_KEYS.HUD_SNAP, position)
          this.positionManager?.applyPosition(position, this.collapsed)
          this.positionManager?.updateArrows(this.collapsed)
        },
        onSizeChange: (size) => {
          GM_setValue(STORAGE_KEYS.HUD_SIZE, size)
        }
      }
    )

    // Attach tab listeners
    if (this.tabManager) {
      this.tabManager.attachListeners()
    }
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

    // Titlebar click to toggle collapse/expand
    this.titlebar.addEventListener('click', (e) => {
      // Don't toggle if clicking on buttons
      if ((e.target as HTMLElement).tagName === 'BUTTON' || (e.target as HTMLElement).closest('button')) return
      this.collapsed = !this.collapsed
      GM_setValue(STORAGE_KEYS.HUD_COLLAPSED, this.collapsed)
      this.updateContentVisibility()
      if (this.positionManager) {
        this.positionManager.updateArrows(this.collapsed)
      }
    })

    // Initialize tab components
    if (this.root) {
      this.hardwareTab = new HardwareTab(
        this.root,
        this.getWsUrl,
        (url) => {
          this.setWsUrl(url)
          this.logInfo(`Device WS URL updated to ${url}`)
        },
        this.onWsUrlChanged,
        () => this.requestDiagnostic()
      )

      this.soundTab = new SoundTab(
        this.root,
        this.soundToggles,
        (t) => this.logInfo(t),
        this.onSoundTest
      )
    }
  }

  private updateContentVisibility() {
    if (!this.content) return

    if (this.collapsed) {
      this.content.style.display = 'none'
    } else {
      this.content.style.display = 'flex'
    }

    if (this.positionManager) {
      this.positionManager.applyPosition(this.positionManager.getPosition(), this.collapsed)
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
    if (!this.settingsPanel || !this.root || !this.positionManager) return

    if (!this.settingsPanel.isVisible()) {
      const hudRect = this.root.getBoundingClientRect()
      const currentPosition = this.positionManager.getPosition()
      this.settingsPanel.positionNextToHud(hudRect, currentPosition)
      this.settingsPanel.show(this.getWsUrl())
      // Hide filter panel if open
      if (this.filterPanel) this.filterPanel.hide()
    } else {
      this.settingsPanel.hide()
    }
  }

  private toggleFilter() {
    if (!this.filterPanel || !this.root || !this.positionManager) return

    if (!this.filterPanel.isVisible()) {
      const hudRect = this.root.getBoundingClientRect()
      const currentPosition = this.positionManager.getPosition()
      this.filterPanel.positionNextToHud(hudRect, currentPosition)
      this.filterPanel.show()
      // Hide settings panel if open
      if (this.settingsPanel) this.settingsPanel.hide()
    } else {
      this.filterPanel.hide()
    }
  }

  private refilterLogs() {
    this.logsTab?.refilterLogs()
  }

  /**
   * Handle tab change callback from TabManager
   */
  private handleTabChange(tabId: TabId) {
    // Refresh hardware display if switching to hardware tab
    if (tabId === 'hardware' && this.hardwareTab && this.hardwareDiagnostic) {
      this.hardwareTab.updateDisplay(this.hardwareDiagnostic)
    }
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
      this.settingsPanel.hide()
    }
    if (this.filterPanel) {
      this.filterPanel.hide()
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
    this.logsTab?.pushLog('send', text)
  }

  logRecv(text: string, eventType?: GameEventType) {
    // Capture hardware diagnostic data
    if (eventType === 'HARDWARE_DIAGNOSTIC') {
      const captured = tryCaptureHardwareDiagnostic(text)
      if (captured) {
        console.log('[OTS HUD] Hardware diagnostic captured:', captured)
        this.hardwareDiagnostic = captured
        this.logInfo('Hardware diagnostic data captured - switch to Hardware tab to view')
        // If currently on hardware tab, update the display
        if (this.tabManager?.getActiveTab() === 'hardware') {
          this.hardwareTab?.updateDisplay(this.hardwareDiagnostic)
        }
      } else {
        console.warn('[OTS HUD] Failed to capture HARDWARE_DIAGNOSTIC:', text)
      }
    }
    this.logsTab?.pushLog('recv', text, eventType)
  }

  logInfo(text: string) {
    this.logsTab?.pushLog('info', text)
  }

  // Alias for compatibility with WsClient
  pushLog(direction: LogDirection, text: string, eventType?: GameEventType) {
    // Capture hardware diagnostic data (same logic as logRecv)
    if (direction === 'recv' && eventType === 'HARDWARE_DIAGNOSTIC') {
      const captured = tryCaptureHardwareDiagnostic(text)
      if (captured) {
        console.log('[OTS HUD] Hardware diagnostic captured:', captured)
        this.hardwareDiagnostic = captured
        this.logInfo('Hardware diagnostic data captured - switch to Hardware tab to view')
        if (this.tabManager?.getActiveTab() === 'hardware') {
          this.hardwareTab?.updateDisplay(this.hardwareDiagnostic)
        }
      } else {
        console.warn('[OTS HUD] Failed to capture HARDWARE_DIAGNOSTIC:', text)
      }
    }
    this.logsTab?.pushLog(direction, text, eventType)
  }
}
