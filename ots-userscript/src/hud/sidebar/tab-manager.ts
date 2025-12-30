/**
 * Tab navigation component for the sidebar HUD
 */

export type TabId = 'logs' | 'hardware' | 'sound'

export type TabConfig = {
  id: TabId
  label: string
  contentId: string
}

export type TabManagerOptions = {
  container: HTMLElement
  tabs: TabConfig[]
  defaultTab: TabId
  onTabChange?: (tabId: TabId) => void
}

export class TabManager {
  private activeTab: TabId
  private tabs: TabConfig[]
  private container: HTMLElement
  private onTabChange?: (tabId: TabId) => void

  constructor(options: TabManagerOptions) {
    this.container = options.container
    this.tabs = options.tabs
    this.activeTab = options.defaultTab
    this.onTabChange = options.onTabChange
  }

  /**
   * Generate HTML for tab navigation buttons
   */
  createTabsHTML(): string {
    return `
      <div id="ots-hud-tabs" style="display:flex;gap:2px;padding:6px 8px;background:rgba(10,10,15,0.6);border-bottom:1px solid rgba(255,255,255,0.1);">
        ${this.tabs
        .map((tab) => {
          const isActive = tab.id === this.activeTab
          const bgColor = isActive ? 'rgba(59,130,246,0.3)' : 'rgba(255,255,255,0.05)'
          const textColor = isActive ? '#e5e7eb' : '#9ca3af'
          return `<button class="ots-tab-btn" data-tab="${tab.id}" style="all:unset;cursor:pointer;font-size:10px;font-weight:600;color:${textColor};padding:4px 12px;border-radius:4px;background:${bgColor};transition:all 0.2s;">${tab.label}</button>`
        })
        .join('')}
      </div>
    `
  }

  /**
   * Attach event listeners to tab buttons
   */
  attachListeners() {
    const tabBtns = this.container.querySelectorAll('.ots-tab-btn')
    tabBtns.forEach((btn) => {
      btn.addEventListener('click', () => {
        const tabId = (btn as HTMLElement).dataset.tab as TabId
        this.switchTab(tabId)
      })

      // Hover effects
      btn.addEventListener('mouseenter', () => {
        const isActive = (btn as HTMLElement).dataset.tab === this.activeTab
        if (!isActive) {
          ; (btn as HTMLElement).style.background = 'rgba(255,255,255,0.1)'
        }
      })
      btn.addEventListener('mouseleave', () => {
        this.updateTabStyles()
      })
    })
  }

  /**
   * Switch to a different tab
   */
  switchTab(tabId: TabId) {
    if (this.activeTab === tabId) return

    this.activeTab = tabId
    this.updateTabStyles()
    this.updateContentVisibility()

    // Notify listener
    if (this.onTabChange) {
      this.onTabChange(tabId)
    }
  }

  /**
   * Update visual styles of tab buttons
   */
  private updateTabStyles() {
    const tabBtns = this.container.querySelectorAll('.ots-tab-btn')
    tabBtns.forEach((btn) => {
      const btnTab = (btn as HTMLElement).dataset.tab
      const isActive = btnTab === this.activeTab
        ; (btn as HTMLElement).style.background = isActive ? 'rgba(59,130,246,0.3)' : 'rgba(255,255,255,0.05)'
        ; (btn as HTMLElement).style.color = isActive ? '#e5e7eb' : '#9ca3af'
    })
  }

  /**
   * Show active tab content, hide others
   */
  private updateContentVisibility() {
    this.tabs.forEach((tab) => {
      const content = this.container.querySelector(`#${tab.contentId}`) as HTMLElement
      if (content) {
        content.style.display = tab.id === this.activeTab ? 'flex' : 'none'
      }
    })
  }

  /**
   * Get the currently active tab
   */
  getActiveTab(): TabId {
    return this.activeTab
  }

  /**
   * Set active tab programmatically
   */
  setActiveTab(tabId: TabId) {
    this.switchTab(tabId)
  }
}
