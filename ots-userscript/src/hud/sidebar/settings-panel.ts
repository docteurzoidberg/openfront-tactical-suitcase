import { FloatingPanel } from './window'

/**
 * SettingsPanel - Dedicated component for WebSocket URL configuration
 * 
 * Provides a floating panel with:
 * - WebSocket URL input field
 * - Reset button (restores default URL)
 * - Save button (persists and applies new URL)
 * - Draggable via header
 */
export class SettingsPanel {
  private panel: HTMLDivElement
  private input: HTMLInputElement
  private floatingPanel: FloatingPanel

  private onClose: () => void
  private onSave: (url: string) => void

  constructor(
    onClose: () => void,
    onSave: (url: string) => void
  ) {
    this.onClose = onClose
    this.onSave = onSave

    // Create panel element
    this.panel = this.createPanel()

    // Get input element
    this.input = this.panel.querySelector('#ots-hud-settings-ws') as HTMLInputElement

    // Setup dragging
    const header = this.panel.querySelector('#ots-hud-settings-header') as HTMLDivElement
    this.floatingPanel = new FloatingPanel(this.panel)
    this.floatingPanel.attachDrag(header)

    // Attach event listeners
    this.attachListeners()

    // Append to body
    document.body.appendChild(this.panel)
  }

  /**
   * Create the settings panel HTML structure
   */
  private createPanel(): HTMLDivElement {
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

    return panel
  }

  /**
   * Attach event listeners to buttons
   */
  private attachListeners() {
    const closeBtn = this.panel.querySelector('#ots-hud-settings-close') as HTMLButtonElement
    closeBtn?.addEventListener('click', () => {
      this.hide()
      this.onClose()
    })

    const resetBtn = this.panel.querySelector('#ots-hud-settings-reset') as HTMLButtonElement
    resetBtn?.addEventListener('click', () => {
      this.input.value = 'ws://localhost:3000/ws'
    })

    const saveBtn = this.panel.querySelector('#ots-hud-settings-save') as HTMLButtonElement
    saveBtn?.addEventListener('click', () => {
      const url = this.input.value.trim()
      if (url) {
        this.hide()
        this.onSave(url)
      }
    })
  }

  /**
   * Show the settings panel with current URL
   */
  show(currentUrl: string) {
    this.input.value = currentUrl
    this.panel.style.display = 'flex'
  }

  /**
   * Hide the settings panel
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
