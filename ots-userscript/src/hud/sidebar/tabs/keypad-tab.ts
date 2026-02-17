import { getDefaultKeypadConfig, loadKeypadConfig, saveKeypadConfig } from '../../../storage/keypad'
import type { KeyBinding, KeypadAction } from '../../../types/keypad-types'

declare const KEYPAD_LAYOUT_SVG: string

const ACTION_OPTIONS: Array<{ value: KeypadAction; label: string }> = [
  { value: 'BUILD_CITY', label: 'Build City' },
  { value: 'BUILD_FACTORY', label: 'Build Factory' },
  { value: 'BUILD_PORT', label: 'Build Port' },
  { value: 'BUILD_DEFENSE', label: 'Build Defense' },
  { value: 'BUILD_MISSILE', label: 'Build Missile Launcher' },
  { value: 'BUILD_SAM', label: 'Build SAM' },
  { value: 'BUILD_WARSHIP', label: 'Build Warship' },
  { value: 'ZOOM_IN', label: 'Zoom In' },
  { value: 'ZOOM_OUT', label: 'Zoom Out' },
  { value: 'ATTACK_DECREASE', label: 'Decrease Attack Ratio' },
  { value: 'MISSILE_SWITCH', label: 'Switch Missile Direction' },
  { value: 'ATTACK_INCREASE', label: 'Increase Attack Ratio' },
  { value: 'BOAT_ATTACK', label: 'Boat Attack' },
  { value: 'LAND_ATTACK', label: 'Land Attack' },
  { value: 'TOGGLE_VIEW', label: 'Toggle View' }
]

export class KeypadTab {
  private container: HTMLElement
  private bindings: KeyBinding[] = []
  private selectedKeyId = 1

  constructor(
    private root: HTMLElement,
    private logInfo: (text: string) => void
  ) {
    this.container = root.querySelector('#ots-keypad-content') as HTMLElement
    if (!this.container) {
      throw new Error('Keypad tab container not found')
    }

    this.render()
  }

  static createHTML(): string {
    return `
      <div id="ots-tab-keypad" class="ots-tab-content" style="display:none;flex:1;overflow-y:auto;padding:8px;background:rgba(10,10,15,0.8);">
        <div id="ots-keypad-content"></div>
      </div>
    `
  }

  private render(): void {
    const config = loadKeypadConfig()
    this.bindings = [...config.bindings].sort((a, b) => a.keyId - b.keyId)
    this.selectedKeyId = this.bindings.find((binding) => binding.enabled)?.keyId ?? 1

    this.container.innerHTML = `
      <div style="margin-bottom:12px;padding:10px;background:rgba(59,130,246,0.08);border-left:3px solid #3b82f6;border-radius:4px;">
        <div style="font-size:11px;font-weight:600;color:#93c5fd;margin-bottom:6px;">⌨ Keypad Bindings</div>
        <div style="font-size:10px;color:#9ca3af;line-height:1.5;">Click a key in the layout to edit its mapping used for KEYPAD_KEY_PRESSED/RELEASED events.</div>
      </div>
      <div style="display:grid;grid-template-columns:minmax(280px,1fr) minmax(220px,300px);gap:10px;align-items:start;">
        <div style="padding:8px;background:rgba(15,23,42,0.5);border:1px solid rgba(148,163,184,0.25);border-radius:4px;">
          <div style="font-size:10px;color:#cbd5e1;font-weight:600;margin-bottom:6px;">Interactive keypad layout</div>
          <div style="font-size:9px;color:#94a3b8;margin-bottom:8px;line-height:1.4;">Select a key to edit. Blue outline = selected, green = enabled, gray = disabled.</div>
          <div id="ots-keypad-layout" style="display:block;width:100%;max-width:480px;overflow:auto;">${KEYPAD_LAYOUT_SVG || ''}</div>
          ${KEYPAD_LAYOUT_SVG
        ? ''
        : `<div style="margin-top:8px;font-size:9px;color:#fbbf24;">Layout SVG not available. Place keyboard-layout.svg in ots-userscript/images and rebuild userscript.</div>`}
        </div>
        <div style="padding:8px;background:rgba(2,6,23,0.65);border:1px solid rgba(148,163,184,0.25);border-radius:4px;">
          <div id="ots-keypad-selected-title" style="font-size:11px;font-weight:700;color:#e5e7eb;margin-bottom:8px;">K1</div>
          <div style="display:flex;flex-direction:column;gap:6px;">
            <select id="ots-keypad-action" style="font-size:10px;padding:5px 6px;border-radius:4px;border:1px solid rgba(148,163,184,0.35);background:rgba(15,23,42,0.8);color:#e5e7eb;outline:none;">
              ${ACTION_OPTIONS.map((action) => `<option value="${action.value}">${action.label}</option>`).join('')}
            </select>
            <input id="ots-keypad-selector" type="text" placeholder="selector" style="font-size:10px;padding:5px 6px;border-radius:4px;border:1px solid rgba(148,163,184,0.35);background:rgba(15,23,42,0.8);color:#e5e7eb;outline:none;" />
            <input id="ots-keypad-hotkey" type="text" placeholder="key" style="font-size:10px;padding:5px 6px;border-radius:4px;border:1px solid rgba(148,163,184,0.35);background:rgba(15,23,42,0.8);color:#e5e7eb;outline:none;" />
            <label style="display:flex;align-items:center;gap:6px;font-size:10px;color:#e5e7eb;cursor:pointer;">
              <input id="ots-keypad-enabled" type="checkbox" />
              <span>Enabled</span>
            </label>
            <div id="ots-keypad-warning" style="display:none;font-size:9px;color:#fbbf24;"></div>
            <button id="ots-keypad-test" style="all:unset;cursor:pointer;font-size:10px;padding:5px 8px;border-radius:4px;background:rgba(59,130,246,0.28);color:#bfdbfe;font-weight:700;text-align:center;">Test selected key</button>
          </div>
        </div>
      </div>
      <div style="display:flex;gap:8px;margin-top:12px;">
        <button id="ots-keypad-save" style="all:unset;cursor:pointer;font-size:11px;padding:6px 12px;border-radius:4px;background:#22c55e;color:#052e16;font-weight:700;">Save</button>
        <button id="ots-keypad-reset" style="all:unset;cursor:pointer;font-size:11px;padding:6px 12px;border-radius:4px;background:#f59e0b;color:#451a03;font-weight:700;">Reset defaults</button>
      </div>
    `

    this.attachListeners()
    this.setupInteractiveLayout()
    this.selectKey(this.selectedKeyId)
  }

  private attachListeners(): void {
    const saveBtn = this.container.querySelector('#ots-keypad-save') as HTMLButtonElement | null
    const resetBtn = this.container.querySelector('#ots-keypad-reset') as HTMLButtonElement | null
    const actionInput = this.container.querySelector('#ots-keypad-action') as HTMLSelectElement | null
    const selectorInput = this.container.querySelector('#ots-keypad-selector') as HTMLInputElement | null
    const hotkeyInput = this.container.querySelector('#ots-keypad-hotkey') as HTMLInputElement | null
    const enabledInput = this.container.querySelector('#ots-keypad-enabled') as HTMLInputElement | null
    const testBtn = this.container.querySelector('#ots-keypad-test') as HTMLButtonElement | null

    actionInput?.addEventListener('change', () => {
      const binding = this.getSelectedBinding()
      if (!binding) return
      binding.action = actionInput.value as KeypadAction
      const selectedAction = ACTION_OPTIONS.find((option) => option.value === binding.action)
      binding.label = selectedAction?.label ?? binding.action
    })

    selectorInput?.addEventListener('input', () => {
      const binding = this.getSelectedBinding()
      if (!binding) return
      binding.selector = selectorInput.value.trim()
      this.updateValidation()
    })

    hotkeyInput?.addEventListener('input', () => {
      const binding = this.getSelectedBinding()
      if (!binding) return
      const hotkeyRaw = hotkeyInput.value.trim()
      binding.hotkey = hotkeyRaw.toLowerCase() === 'space' ? ' ' : hotkeyRaw
      this.updateLayoutVisuals()
      this.updateValidation()
    })

    enabledInput?.addEventListener('change', () => {
      const binding = this.getSelectedBinding()
      if (!binding) return
      binding.enabled = enabledInput.checked
      this.updateLayoutVisuals()
      this.updateValidation()
    })

    testBtn?.addEventListener('click', () => {
      const binding = this.getSelectedBinding()
      if (!binding) return
      this.triggerBinding(binding)
    })

    saveBtn?.addEventListener('click', () => {
      const updated = [...this.bindings].sort((a, b) => a.keyId - b.keyId)
      if (updated.length !== 15) {
        this.logInfo('Keypad config save failed: invalid row count')
        return
      }
      saveKeypadConfig({ version: 1, bindings: updated })
      this.logInfo('Keypad bindings saved')
    })

    resetBtn?.addEventListener('click', () => {
      const defaults = getDefaultKeypadConfig()
      saveKeypadConfig(defaults)
      this.render()
      this.logInfo('Keypad bindings reset to defaults')
    })
  }

  private setupInteractiveLayout(): void {
    const layoutContainer = this.container.querySelector('#ots-keypad-layout') as HTMLElement | null
    if (!layoutContainer) return

    const svg = layoutContainer.querySelector('svg') as SVGSVGElement | null
    if (!svg) return

    svg.style.width = '100%'
    svg.style.height = 'auto'

    const groups = Array.from(svg.querySelectorAll('g.keycap')) as SVGGElement[]
    groups.slice(0, 15).forEach((group, index) => {
      const keyId = index + 1
      group.setAttribute('data-key-id', String(keyId))
      group.style.cursor = 'pointer'
      group.addEventListener('click', () => this.selectKey(keyId))
      this.injectKeyLabel(group, keyId)
    })

    this.updateLayoutVisuals()
  }

  private injectKeyLabel(group: SVGGElement, keyId: number): void {
    const existing = group.querySelector('[data-role="ots-key-label"]')
    if (existing) return

    const rects = group.querySelectorAll('rect')
    if (rects.length < 4) return

    const innerRect = rects[3]
    const x = Number(innerRect.getAttribute('x') ?? 0)
    const y = Number(innerRect.getAttribute('y') ?? 0)
    const width = Number(innerRect.getAttribute('width') ?? 0)
    const height = Number(innerRect.getAttribute('height') ?? 0)

    const label = document.createElementNS('http://www.w3.org/2000/svg', 'text')
    label.setAttribute('x', String(x + width / 2))
    label.setAttribute('y', String(y + (height / 2) + 3))
    label.setAttribute('text-anchor', 'middle')
    label.setAttribute('font-size', '11')
    label.setAttribute('font-weight', '700')
    label.setAttribute('fill', '#0f172a')
    label.setAttribute('pointer-events', 'none')
    label.setAttribute('data-role', 'ots-key-label')
    label.textContent = `K${keyId}`
    group.appendChild(label)

    const hotkey = document.createElementNS('http://www.w3.org/2000/svg', 'text')
    hotkey.setAttribute('x', String(x + width / 2))
    hotkey.setAttribute('y', String(y + height - 3))
    hotkey.setAttribute('text-anchor', 'middle')
    hotkey.setAttribute('font-size', '8')
    hotkey.setAttribute('font-weight', '600')
    hotkey.setAttribute('fill', '#334155')
    hotkey.setAttribute('pointer-events', 'none')
    hotkey.setAttribute('data-role', 'ots-hotkey-label')
    group.appendChild(hotkey)
  }

  private updateLayoutVisuals(): void {
    const groups = this.container.querySelectorAll('g.keycap[data-key-id]')
    groups.forEach((groupEl) => {
      const group = groupEl as SVGGElement
      const keyId = Number(group.getAttribute('data-key-id'))
      const binding = this.bindings.find((item) => item.keyId === keyId)
      const rects = group.querySelectorAll('rect')
      if (rects.length < 4 || !binding) return

      const borderRect = rects[0]
      const innerFillRect = rects[3]
      const hotkeyLabel = group.querySelector('[data-role="ots-hotkey-label"]') as SVGTextElement | null

      borderRect.setAttribute('stroke', keyId === this.selectedKeyId ? '#3b82f6' : '#000000')
      borderRect.setAttribute('stroke-width', keyId === this.selectedKeyId ? '3' : '2')

      innerFillRect.setAttribute('fill', binding.enabled ? '#bbf7d0' : '#e5e7eb')
      if (hotkeyLabel) {
        hotkeyLabel.textContent = binding.hotkey === ' ' ? 'Space' : (binding.hotkey || '')
      }

      group.setAttribute('title', `K${keyId} · ${binding.label ?? binding.action}${binding.enabled ? '' : ' (disabled)'}`)
    })
  }

  private selectKey(keyId: number): void {
    this.selectedKeyId = keyId
    const binding = this.getSelectedBinding()
    if (!binding) return

    const title = this.container.querySelector('#ots-keypad-selected-title') as HTMLElement | null
    const actionInput = this.container.querySelector('#ots-keypad-action') as HTMLSelectElement | null
    const selectorInput = this.container.querySelector('#ots-keypad-selector') as HTMLInputElement | null
    const hotkeyInput = this.container.querySelector('#ots-keypad-hotkey') as HTMLInputElement | null
    const enabledInput = this.container.querySelector('#ots-keypad-enabled') as HTMLInputElement | null

    if (title) {
      title.textContent = `K${binding.keyId} · ${binding.label ?? binding.action}`
    }
    if (actionInput) actionInput.value = binding.action
    if (selectorInput) selectorInput.value = binding.selector
    if (hotkeyInput) hotkeyInput.value = binding.hotkey === ' ' ? 'Space' : binding.hotkey
    if (enabledInput) enabledInput.checked = binding.enabled

    this.updateLayoutVisuals()
    this.updateValidation()
  }

  private getSelectedBinding(): KeyBinding | undefined {
    return this.bindings.find((binding) => binding.keyId === this.selectedKeyId)
  }

  private updateValidation(): void {
    const binding = this.getSelectedBinding()
    const warning = this.container.querySelector('#ots-keypad-warning') as HTMLElement | null
    if (!warning) return
    if (!binding) {
      warning.style.display = 'none'
      return
    }

    if (!binding.enabled) {
      warning.style.display = 'none'
      return
    }

    if (!binding.selector && !binding.hotkey) {
      warning.textContent = 'Enabled but no selector/hotkey configured'
      warning.style.display = 'block'
      return
    }

    warning.style.display = 'none'
  }

  private triggerBinding(binding: KeyBinding): void {
    if (!binding.enabled) {
      this.logInfo(`K${binding.keyId} is disabled`)
      return
    }

    const element = binding.selector ? (document.querySelector(binding.selector) as HTMLElement | null) : null
    if (element) {
      element.click()
      this.logInfo(`Test K${binding.keyId}: clicked selector`)
      return
    }

    if (binding.hotkey) {
      const key = binding.hotkey
      const code = key === ' '
        ? 'Space'
        : key.length === 1 && key >= 'a' && key <= 'z'
          ? `Key${key.toUpperCase()}`
          : key.length === 1 && key >= '0' && key <= '9'
            ? `Digit${key}`
            : key

      const down = new KeyboardEvent('keydown', { key, code, bubbles: true, cancelable: true })
      const up = new KeyboardEvent('keyup', { key, code, bubbles: true, cancelable: true })
      document.dispatchEvent(down)
      document.dispatchEvent(up)
      this.logInfo(`Test K${binding.keyId}: sent hotkey ${key === ' ' ? 'Space' : key}`)
      return
    }

    this.logInfo(`Test K${binding.keyId}: no selector/hotkey configured`)
  }

}
