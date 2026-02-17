import { getDefaultKeypadConfig, loadKeypadConfig, saveKeypadConfig } from '../../../storage/keypad'
import type { KeyBinding, KeypadAction } from '../../../types/keypad-types'

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
    const sorted = [...config.bindings].sort((a, b) => a.keyId - b.keyId)

    this.container.innerHTML = `
      <div style="margin-bottom:12px;padding:10px;background:rgba(59,130,246,0.08);border-left:3px solid #3b82f6;border-radius:4px;">
        <div style="font-size:11px;font-weight:600;color:#93c5fd;margin-bottom:6px;">⌨ Keypad Bindings</div>
        <div style="font-size:10px;color:#9ca3af;line-height:1.5;">Edit key mappings used when KEYPAD_KEY_PRESSED/RELEASED events arrive from firmware.</div>
      </div>
      <div style="display:grid;grid-template-columns:46px 1fr 84px;gap:6px;align-items:center;font-size:9px;color:#94a3b8;margin-bottom:6px;font-weight:600;letter-spacing:0.05em;">
        <div>KEY</div><div>MAPPING</div><div>ENABLE</div>
      </div>
      <div id="ots-keypad-rows" style="display:flex;flex-direction:column;gap:6px;"></div>
      <div style="display:flex;gap:8px;margin-top:12px;">
        <button id="ots-keypad-save" style="all:unset;cursor:pointer;font-size:11px;padding:6px 12px;border-radius:4px;background:#22c55e;color:#052e16;font-weight:700;">Save</button>
        <button id="ots-keypad-reset" style="all:unset;cursor:pointer;font-size:11px;padding:6px 12px;border-radius:4px;background:#f59e0b;color:#451a03;font-weight:700;">Reset defaults</button>
      </div>
    `

    const rows = this.container.querySelector('#ots-keypad-rows') as HTMLElement
    rows.innerHTML = sorted.map((binding) => this.rowHTML(binding)).join('')
    this.attachListeners()
  }

  private rowHTML(binding: KeyBinding): string {
    const options = ACTION_OPTIONS
      .map((action) => `<option value="${action.value}" ${action.value === binding.action ? 'selected' : ''}>${action.label}</option>`)
      .join('')

    return `
      <div data-key-id="${binding.keyId}" style="display:grid;grid-template-columns:46px 1fr 96px;gap:6px;align-items:start;padding:6px;background:rgba(255,255,255,0.04);border-radius:4px;">
        <div style="font-size:11px;font-weight:700;color:#e5e7eb;">K${binding.keyId}</div>
        <div>
          <div style="display:grid;grid-template-columns:1fr 96px 56px;gap:6px;">
          <select data-field="action" style="font-size:10px;padding:4px 6px;border-radius:4px;border:1px solid rgba(148,163,184,0.35);background:rgba(15,23,42,0.8);color:#e5e7eb;outline:none;">
            ${options}
          </select>
          <input data-field="selector" type="text" value="${binding.selector.replace(/"/g, '&quot;')}" placeholder="selector" style="font-size:10px;padding:4px 6px;border-radius:4px;border:1px solid rgba(148,163,184,0.35);background:rgba(15,23,42,0.8);color:#e5e7eb;outline:none;" />
          <input data-field="hotkey" type="text" value="${binding.hotkey === ' ' ? 'Space' : binding.hotkey}" placeholder="key" style="font-size:10px;padding:4px 6px;border-radius:4px;border:1px solid rgba(148,163,184,0.35);background:rgba(15,23,42,0.8);color:#e5e7eb;outline:none;" />
          </div>
          <div data-role="warning" style="display:none;margin-top:4px;font-size:9px;color:#fbbf24;"></div>
        </div>
        <div style="display:flex;align-items:center;gap:8px;padding-top:2px;">
          <label style="display:flex;align-items:center;gap:6px;font-size:10px;color:#e5e7eb;cursor:pointer;justify-self:start;">
            <input data-field="enabled" type="checkbox" ${binding.enabled ? 'checked' : ''} />
            <span>On</span>
          </label>
          <button data-field="test" style="all:unset;cursor:pointer;font-size:10px;padding:4px 8px;border-radius:4px;background:rgba(59,130,246,0.28);color:#bfdbfe;font-weight:700;">Test</button>
        </div>
      </div>
    `
  }

  private attachListeners(): void {
    const saveBtn = this.container.querySelector('#ots-keypad-save') as HTMLButtonElement | null
    const resetBtn = this.container.querySelector('#ots-keypad-reset') as HTMLButtonElement | null

    const rows = this.container.querySelectorAll('[data-key-id]')
    rows.forEach((rowEl) => {
      const row = rowEl as HTMLElement
      const selectorInput = row.querySelector('[data-field="selector"]') as HTMLInputElement
      const hotkeyInput = row.querySelector('[data-field="hotkey"]') as HTMLInputElement
      const enabledInput = row.querySelector('[data-field="enabled"]') as HTMLInputElement
      const testBtn = row.querySelector('[data-field="test"]') as HTMLButtonElement

      const validate = () => this.updateRowValidation(row)

      selectorInput.addEventListener('input', validate)
      hotkeyInput.addEventListener('input', validate)
      enabledInput.addEventListener('change', validate)
      validate()

      testBtn.addEventListener('click', () => {
        const binding = this.readRow(row)
        this.triggerBinding(binding)
      })
    })

    saveBtn?.addEventListener('click', () => {
      const updated = this.readRows()
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

  private updateRowValidation(row: HTMLElement): void {
    const binding = this.readRow(row)
    const warning = row.querySelector('[data-role="warning"]') as HTMLElement | null
    if (!warning) return

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

  private readRow(row: HTMLElement): KeyBinding {
    const keyId = Number(row.dataset.keyId)
    const action = (row.querySelector('[data-field="action"]') as HTMLSelectElement).value as KeypadAction
    const selector = (row.querySelector('[data-field="selector"]') as HTMLInputElement).value.trim()
    const hotkeyRaw = (row.querySelector('[data-field="hotkey"]') as HTMLInputElement).value.trim()
    const enabled = (row.querySelector('[data-field="enabled"]') as HTMLInputElement).checked
    const hotkey = hotkeyRaw.toLowerCase() === 'space' ? ' ' : hotkeyRaw
    const selectedAction = ACTION_OPTIONS.find((option) => option.value === action)

    return {
      keyId,
      action,
      selector,
      hotkey,
      enabled,
      label: selectedAction?.label ?? action
    }
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

  private readRows(): KeyBinding[] {
    const rows = this.container.querySelectorAll('[data-key-id]')
    const bindings: KeyBinding[] = []

    rows.forEach((rowEl) => {
      const row = rowEl as HTMLElement
      bindings.push(this.readRow(row))
    })

    return bindings.sort((a, b) => a.keyId - b.keyId)
  }
}
