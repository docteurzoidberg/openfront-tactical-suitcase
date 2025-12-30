import { FloatingPanel } from './window'
import type { LogFilters } from './types'
import { DEFAULT_LOG_FILTERS } from './constants'

/**
 * FilterPanel - Dedicated component for log filtering configuration
 * 
 * Provides a floating panel with:
 * - Direction filters (send, recv, info)
 * - Event type filters (game, nukes, alerts, troops, sounds, system, heartbeat)
 * - Real-time filter updates with persistence
 * - Draggable via header
 */
export class FilterPanel {
  private panel: HTMLDivElement
  private floatingPanel: FloatingPanel
  private checkboxes: {
    directions: { send: HTMLInputElement, recv: HTMLInputElement, info: HTMLInputElement }
    events: { game: HTMLInputElement, nukes: HTMLInputElement, alerts: HTMLInputElement, troops: HTMLInputElement, sounds: HTMLInputElement, system: HTMLInputElement, heartbeat: HTMLInputElement }
  }

  private onClose: () => void
  private onFilterChange: (filters: LogFilters) => void

  constructor(
    onClose: () => void,
    onFilterChange: (filters: LogFilters) => void,
    initialFilters: LogFilters
  ) {
    this.onClose = onClose
    this.onFilterChange = onFilterChange

    // Create panel element
    this.panel = this.createPanel()

    // Get checkbox elements
    this.checkboxes = {
      directions: {
        send: this.panel.querySelector('#ots-hud-filter-send') as HTMLInputElement,
        recv: this.panel.querySelector('#ots-hud-filter-recv') as HTMLInputElement,
        info: this.panel.querySelector('#ots-hud-filter-info') as HTMLInputElement
      },
      events: {
        game: this.panel.querySelector('#ots-hud-filter-game') as HTMLInputElement,
        nukes: this.panel.querySelector('#ots-hud-filter-nukes') as HTMLInputElement,
        alerts: this.panel.querySelector('#ots-hud-filter-alerts') as HTMLInputElement,
        troops: this.panel.querySelector('#ots-hud-filter-troops') as HTMLInputElement,
        sounds: this.panel.querySelector('#ots-hud-filter-sounds') as HTMLInputElement,
        system: this.panel.querySelector('#ots-hud-filter-system') as HTMLInputElement,
        heartbeat: this.panel.querySelector('#ots-hud-filter-heartbeat') as HTMLInputElement
      }
    }

    // Restore initial filter states
    this.restoreFilterStates(initialFilters)

    // Setup dragging
    const header = this.panel.querySelector('#ots-hud-filter-header') as HTMLDivElement
    this.floatingPanel = new FloatingPanel(this.panel)
    this.floatingPanel.attachDrag(header)

    // Attach event listeners
    this.attachListeners()

    // Append to body
    document.body.appendChild(this.panel)
  }

  /**
   * Create the filter panel HTML structure
   */
  private createPanel(): HTMLDivElement {
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

    return panel
  }

  /**
   * Attach event listeners to checkboxes and close button
   */
  private attachListeners() {
    const closeBtn = this.panel.querySelector('#ots-hud-filter-close') as HTMLButtonElement
    closeBtn?.addEventListener('click', () => {
      this.hide()
      this.onClose()
    })

    // Direction checkboxes
    Object.entries(this.checkboxes.directions).forEach(([key, checkbox]) => {
      checkbox.addEventListener('change', () => {
        this.onFilterChange(this.getCurrentFilters())
      })
    })

    // Event checkboxes
    Object.entries(this.checkboxes.events).forEach(([key, checkbox]) => {
      checkbox.addEventListener('change', () => {
        this.onFilterChange(this.getCurrentFilters())
      })
    })
  }

  /**
   * Restore filter checkbox states from LogFilters
   */
  private restoreFilterStates(filters: LogFilters) {
    this.checkboxes.directions.send.checked = filters.directions.send
    this.checkboxes.directions.recv.checked = filters.directions.recv
    this.checkboxes.directions.info.checked = filters.directions.info
    this.checkboxes.events.game.checked = filters.events.game
    this.checkboxes.events.nukes.checked = filters.events.nukes
    this.checkboxes.events.alerts.checked = filters.events.alerts
    this.checkboxes.events.troops.checked = filters.events.troops
    this.checkboxes.events.sounds.checked = filters.events.sounds
    this.checkboxes.events.system.checked = filters.events.system
    this.checkboxes.events.heartbeat.checked = filters.events.heartbeat
  }

  /**
   * Get current filter state from checkboxes
   */
  private getCurrentFilters(): LogFilters {
    return {
      directions: {
        send: this.checkboxes.directions.send.checked,
        recv: this.checkboxes.directions.recv.checked,
        info: this.checkboxes.directions.info.checked
      },
      events: {
        game: this.checkboxes.events.game.checked,
        nukes: this.checkboxes.events.nukes.checked,
        alerts: this.checkboxes.events.alerts.checked,
        troops: this.checkboxes.events.troops.checked,
        sounds: this.checkboxes.events.sounds.checked,
        system: this.checkboxes.events.system.checked,
        heartbeat: this.checkboxes.events.heartbeat.checked
      }
    }
  }

  /**
   * Show the filter panel
   */
  show() {
    this.panel.style.display = 'flex'
  }

  /**
   * Hide the filter panel
   */
  hide() {
    this.panel.style.display = 'none'
  }

  /**
   * Check if panel is currently visible
   */
  isVisible(): boolean {
    return this.panel.style.display === 'flex'
  }

  /**
   * Position the panel next to the HUD
   * @param hudRect - Bounding rectangle of the HUD
   * @param position - Current HUD position (right, left, top, bottom)
   */
  positionNextToHud(hudRect: DOMRect, position: string) {
    const gap = 8

    // Clear all position properties
    this.panel.style.top = ''
    this.panel.style.bottom = ''
    this.panel.style.left = ''
    this.panel.style.right = ''

    // Position based on HUD location
    switch (position) {
      case 'right':
        // HUD on right edge, panel goes to the left
        this.panel.style.top = `${hudRect.top}px`
        this.panel.style.right = `${window.innerWidth - hudRect.left + gap}px`
        break
      case 'left':
        // HUD on left edge, panel goes to the right
        this.panel.style.top = `${hudRect.top}px`
        this.panel.style.left = `${hudRect.right + gap}px`
        break
      case 'top':
        // HUD on top edge, panel goes below
        this.panel.style.top = `${hudRect.bottom + gap}px`
        this.panel.style.left = `${hudRect.left}px`
        break
      case 'bottom':
        // HUD on bottom edge, panel goes above
        this.panel.style.bottom = `${window.innerHeight - hudRect.top + gap}px`
        this.panel.style.left = `${hudRect.left}px`
        break
    }
  }

  /**
   * Get the panel element (for positioning)
   */
  getElement(): HTMLDivElement {
    return this.panel
  }
}
