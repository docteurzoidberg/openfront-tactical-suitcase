import { getDefaultKeypadConfig, loadKeypadConfig, saveKeypadConfig } from '../../../storage/keypad'
import type { KeyBinding, KeypadAction, KeypadKeyEventData } from '../../../types/keypad-types'

declare const KEYPAD_LAYOUT_SVG: string

const GAME_KEYBIND_LABELS: Record<string, string> = {
  buildCity: 'Build City',
  buildFactory: 'Build Factory',
  buildPort: 'Build Port',
  buildDefensePost: 'Build Defense Post',
  buildMissileSilo: 'Build Missile Launcher',
  buildSamLauncher: 'Build SAM Launcher',
  buildWarship: 'Build Warship',
  zoomIn: 'Zoom In',
  zoomOut: 'Zoom Out',
  attackRatioDown: 'Decrease Attack Ratio',
  attackRatioUp: 'Increase Attack Ratio',
  swapDirection: 'Switch Missile Direction',
  boatAttack: 'Boat Attack',
  groundAttack: 'Land Attack',
  toggleView: 'Toggle View',
  centerCamera: 'Center Camera',
  moveUp: 'Move Up',
  moveDown: 'Move Down',
  moveLeft: 'Move Left',
  moveRight: 'Move Right'
}

const DEFAULT_GAME_HOTKEYS: Record<string, string> = {
  buildCity: '1',
  buildFactory: '2',
  buildPort: '3',
  buildDefensePost: '4',
  buildMissileSilo: '5',
  buildSamLauncher: '6',
  buildWarship: '7',
  zoomIn: 'e',
  zoomOut: 'q',
  attackRatioDown: 't',
  attackRatioUp: 'y',
  swapDirection: 'u',
  boatAttack: 'b',
  groundAttack: 'g',
  toggleView: ' '
}

const DEFAULT_GAME_KEYBIND_CODES: Record<string, string> = {
  toggleView: 'Space',
  centerCamera: 'KeyC',
  moveUp: 'KeyW',
  moveDown: 'KeyS',
  moveLeft: 'KeyA',
  moveRight: 'KeyD',
  zoomOut: 'KeyQ',
  zoomIn: 'KeyE',
  attackRatioDown: 'KeyT',
  attackRatioUp: 'KeyY',
  boatAttack: 'KeyB',
  groundAttack: 'KeyG',
  swapDirection: 'KeyU',
  buildCity: 'Digit1',
  buildFactory: 'Digit2',
  buildPort: 'Digit3',
  buildDefensePost: 'Digit4',
  buildMissileSilo: 'Digit5',
  buildSamLauncher: 'Digit6',
  buildWarship: 'Digit7',
  buildAtomBomb: 'Digit8',
  buildHydrogenBomb: 'Digit9',
  buildMIRV: 'Digit0'
}

export class KeypadTab {
  private container: HTMLElement
  private bindings: KeyBinding[] = []
  private selectedKeyId: number | null = null
  private livePressedKeys = new Set<number>()

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
    this.selectedKeyId = null

    this.container.innerHTML = `
      <div style="margin-bottom:12px;padding:10px;background:rgba(59,130,246,0.08);border-left:3px solid #3b82f6;border-radius:4px;">
        <div style="font-size:11px;font-weight:600;color:#93c5fd;margin-bottom:6px;">⌨ Keypad Bindings</div>
        <div style="font-size:10px;color:#9ca3af;line-height:1.5;">Click a key in the layout to edit its mapping used for KEYPAD_KEY_PRESSED/RELEASED events.</div>
      </div>
      <div style="display:flex;flex-direction:column;gap:10px;align-items:stretch;">
        <div style="padding:8px;background:rgba(15,23,42,0.5);border:1px solid rgba(148,163,184,0.25);border-radius:4px;">
          <div style="font-size:10px;color:#cbd5e1;font-weight:600;margin-bottom:6px;">Interactive keypad layout</div>
          <div style="font-size:9px;color:#94a3b8;margin-bottom:8px;line-height:1.4;">Select a key to edit. Blue outline = selected, green = enabled, gray = disabled.</div>
          <div id="ots-keypad-layout" style="display:block;width:100%;overflow:auto;">${KEYPAD_LAYOUT_SVG || ''}</div>
          ${KEYPAD_LAYOUT_SVG
        ? ''
        : `<div style="margin-top:8px;font-size:9px;color:#fbbf24;">Layout SVG not available. Place keyboard-layout.svg in ots-userscript/images and rebuild userscript.</div>`}
        </div>
        <div id="ots-keypad-editor" style="display:none;padding:8px;background:rgba(2,6,23,0.65);border:1px solid rgba(148,163,184,0.25);border-radius:4px;">
          <div id="ots-keypad-selected-title" style="font-size:11px;font-weight:700;color:#e5e7eb;margin-bottom:8px;">Select a key</div>
          <div style="display:flex;flex-direction:column;gap:6px;">
            <div style="font-size:10px;color:#cbd5e1;">Assigned keyboard key: <span id="ots-keypad-current-hotkey" style="font-weight:700;color:#f8fafc;">-</span></div>
            <div style="display:flex;flex-direction:column;gap:6px;">
              <select id="ots-keypad-openfront-bind" style="font-size:12px;padding:8px 10px;border-radius:4px;border:1px solid rgba(148,163,184,0.35);background:rgba(15,23,42,0.8);color:#e5e7eb;outline:none;">
                ${this.renderOpenFrontKeybindOptions()}
              </select>
              <button id="ots-keypad-apply-openfront-bind" style="all:unset;display:none;cursor:pointer;font-size:10px;padding:5px 8px;border-radius:4px;background:rgba(14,165,233,0.25);color:#bae6fd;font-weight:700;text-align:center;">Set action</button>
            </div>
            <button id="ots-keypad-disable" style="all:unset;cursor:pointer;font-size:10px;padding:5px 8px;border-radius:4px;background:rgba(245,158,11,0.22);color:#fde68a;font-weight:700;text-align:center;">Disable</button>
            <div id="ots-keypad-warning" style="display:none;font-size:9px;color:#fbbf24;"></div>
            <button id="ots-keypad-reset-selected" style="all:unset;cursor:pointer;font-size:10px;padding:5px 8px;border-radius:4px;background:rgba(245,158,11,0.22);color:#fde68a;font-weight:700;text-align:center;">Reset selected key</button>
            <button id="ots-keypad-test" style="all:unset;cursor:pointer;font-size:10px;padding:5px 8px;border-radius:4px;background:rgba(59,130,246,0.28);color:#bfdbfe;font-weight:700;text-align:center;">Test selected key</button>
          </div>
        </div>
      </div>
      <div style="display:flex;gap:8px;margin-top:12px;width:100%;">
        <button id="ots-keypad-reset" style="all:unset;cursor:pointer;font-size:11px;padding:6px 12px;border-radius:4px;background:#f59e0b;color:#451a03;font-weight:700;text-align:center;width:100%;">Reset defaults</button>
      </div>
    `

    this.attachListeners()
    this.setupInteractiveLayout()
    this.setEditorVisible(false)
    this.setEditorEnabled(false)
    this.updateLayoutVisuals()
  }

  private attachListeners(): void {
    const resetBtn = this.container.querySelector('#ots-keypad-reset') as HTMLButtonElement | null
    const openFrontBindInput = this.container.querySelector('#ots-keypad-openfront-bind') as HTMLSelectElement | null
    const applyOpenFrontBindBtn = this.container.querySelector('#ots-keypad-apply-openfront-bind') as HTMLButtonElement | null
    const disableBtn = this.container.querySelector('#ots-keypad-disable') as HTMLButtonElement | null
    const resetSelectedBtn = this.container.querySelector('#ots-keypad-reset-selected') as HTMLButtonElement | null
    const testBtn = this.container.querySelector('#ots-keypad-test') as HTMLButtonElement | null

    openFrontBindInput?.addEventListener('change', () => {
      this.updateActionApplyState()
    })

    applyOpenFrontBindBtn?.addEventListener('click', () => {
      const binding = this.getSelectedBinding()
      if (!binding) return
      const selected = openFrontBindInput?.value ?? ''
      if (!selected) return

      if (selected === '__DISABLED__') {
        binding.enabled = false
        this.updateSelectionUI(binding)
        this.updateLayoutVisuals()
        this.updateValidation()
        this.updateActionApplyState()
        this.persistCurrentBindings()
        this.logInfo(`K${binding.keyId} disabled`)
        return
      }

      this.assignActionToSelected(selected)
      this.persistCurrentBindings()
      this.logInfo(`K${binding.keyId} assigned to ${binding.label}`)
      this.updateActionApplyState()
    })

    disableBtn?.addEventListener('click', () => {
      const binding = this.getSelectedBinding()
      if (!binding) return

      binding.enabled = false
      this.updateSelectionUI(binding)
      this.updateLayoutVisuals()
      this.updateValidation()
      this.updateActionApplyState()
      this.logInfo(`K${binding.keyId} disabled`)
    })

    resetSelectedBtn?.addEventListener('click', () => {
      this.resetSelectedKeyToDefault()
    })

    testBtn?.addEventListener('click', () => {
      const binding = this.getSelectedBinding()
      if (!binding) return
      this.triggerBinding(binding)
    })

    resetBtn?.addEventListener('click', () => {
      const confirmed = window.confirm('Reset all key bindings to defaults?')
      if (!confirmed) {
        return
      }
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
    const ordered = groups
      .slice(0, 15)
      .map((group) => {
        const rect = group.querySelector('rect')
        const x = Number(rect?.getAttribute('x') ?? 0)
        const y = Number(rect?.getAttribute('y') ?? 0)
        const row = Math.round(y / 50)
        return { group, x, row }
      })
      .sort((a, b) => {
        if (a.row !== b.row) return a.row - b.row
        return a.x - b.x
      })

    ordered.forEach((entry, index) => {
      const group = entry.group
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
      const runtimeHotkey = this.getRuntimeHotkeyForBinding(binding)
      const isPressed = this.livePressedKeys.has(keyId)

      const isSelected = this.selectedKeyId !== null && keyId === this.selectedKeyId
      borderRect.setAttribute('stroke', isSelected ? '#3b82f6' : '#000000')
      borderRect.setAttribute('stroke-width', isSelected ? '3' : '2')

      const fillColor = isPressed
        ? '#93c5fd'
        : !binding.enabled
          ? '#e5e7eb'
          : '#bbf7d0'

      innerFillRect.setAttribute('fill', fillColor)
      innerFillRect.style.transition = 'fill 90ms ease-out, transform 90ms ease-out'
      innerFillRect.style.transform = isPressed ? 'translateY(1px)' : 'translateY(0px)'

      group.style.transition = 'filter 90ms ease-out'
      group.style.filter = isPressed ? 'drop-shadow(0 0 6px rgba(147,197,253,0.95))' : 'none'

      if (hotkeyLabel) {
        hotkeyLabel.textContent = runtimeHotkey === ' ' ? 'Space' : (runtimeHotkey || '')
        hotkeyLabel.setAttribute('fill', isPressed ? '#0f172a' : '#334155')
      }

      group.setAttribute('title', `K${keyId} · ${binding.label ?? binding.action}${binding.enabled ? '' : ' (disabled)'}`)
    })
  }

  private selectKey(keyId: number): void {
    this.selectedKeyId = keyId
    const binding = this.getSelectedBinding()
    if (!binding) return

    this.setEditorVisible(true)
    this.setEditorEnabled(true)

    const title = this.container.querySelector('#ots-keypad-selected-title') as HTMLElement | null
    if (title) {
      title.textContent = `K${binding.keyId} · ${binding.label ?? binding.action}`
    }

    this.updateSelectionUI(binding)

    this.updateLayoutVisuals()
    this.updateValidation()
    this.updateActionApplyState()
  }

  private updateSelectionUI(binding: KeyBinding): void {
    const currentHotkey = this.container.querySelector('#ots-keypad-current-hotkey') as HTMLElement | null
    const openFrontBindInput = this.container.querySelector('#ots-keypad-openfront-bind') as HTMLSelectElement | null

    if (currentHotkey) currentHotkey.textContent = this.formatHotkey(this.getRuntimeHotkeyForBinding(binding))

    if (openFrontBindInput) {
      if (!binding.enabled) {
        openFrontBindInput.value = '__DISABLED__'
      } else {
        openFrontBindInput.value = binding.action
      }
    }
  }

  private getSelectedBinding(): KeyBinding | undefined {
    if (this.selectedKeyId === null) {
      return undefined
    }
    return this.bindings.find((binding) => binding.keyId === this.selectedKeyId)
  }

  private setEditorVisible(visible: boolean): void {
    const editor = this.container.querySelector('#ots-keypad-editor') as HTMLElement | null
    if (editor) {
      editor.style.display = visible ? 'block' : 'none'
    }
  }

  private setEditorEnabled(enabled: boolean): void {
    const openFrontBindInput = this.container.querySelector('#ots-keypad-openfront-bind') as HTMLSelectElement | null
    const applyOpenFrontBindBtn = this.container.querySelector('#ots-keypad-apply-openfront-bind') as HTMLButtonElement | null
    const disableBtn = this.container.querySelector('#ots-keypad-disable') as HTMLButtonElement | null
    const currentHotkey = this.container.querySelector('#ots-keypad-current-hotkey') as HTMLElement | null
    const resetSelectedBtn = this.container.querySelector('#ots-keypad-reset-selected') as HTMLButtonElement | null
    const testBtn = this.container.querySelector('#ots-keypad-test') as HTMLButtonElement | null

    if (openFrontBindInput) openFrontBindInput.disabled = !enabled
    if (applyOpenFrontBindBtn) applyOpenFrontBindBtn.disabled = !enabled
    if (disableBtn) disableBtn.disabled = !enabled
    if (currentHotkey && !enabled) currentHotkey.textContent = '-'
    if (applyOpenFrontBindBtn) {
      if (!enabled) {
        applyOpenFrontBindBtn.style.display = 'none'
      }
      applyOpenFrontBindBtn.style.opacity = enabled ? '1' : '0.5'
      applyOpenFrontBindBtn.style.cursor = enabled ? 'pointer' : 'default'
    }
    if (disableBtn) {
      disableBtn.style.opacity = enabled ? '1' : '0.5'
      disableBtn.style.cursor = enabled ? 'pointer' : 'default'
    }
    if (resetSelectedBtn) {
      resetSelectedBtn.disabled = !enabled
      resetSelectedBtn.style.opacity = enabled ? '1' : '0.5'
      resetSelectedBtn.style.cursor = enabled ? 'pointer' : 'default'
    }
    if (testBtn) {
      testBtn.disabled = !enabled
      testBtn.style.opacity = enabled ? '1' : '0.5'
      testBtn.style.cursor = enabled ? 'pointer' : 'default'
    }
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

    warning.style.display = 'none'
  }

  private persistCurrentBindings(): void {
    const updated = [...this.bindings].sort((a, b) => a.keyId - b.keyId)
    if (updated.length !== 15) {
      this.logInfo('Keypad config save failed: invalid row count')
      return
    }

    saveKeypadConfig({ version: 1, bindings: updated })
  }

  private updateActionApplyState(): void {
    const binding = this.getSelectedBinding()
    const openFrontBindInput = this.container.querySelector('#ots-keypad-openfront-bind') as HTMLSelectElement | null
    const applyOpenFrontBindBtn = this.container.querySelector('#ots-keypad-apply-openfront-bind') as HTMLButtonElement | null

    if (!binding || !openFrontBindInput || !applyOpenFrontBindBtn) {
      return
    }

    const selected = openFrontBindInput.value
    const current = binding.enabled ? binding.action : '__DISABLED__'
    const hasChanged = selected !== current

    applyOpenFrontBindBtn.style.display = hasChanged ? 'block' : 'none'
  }

  private assignActionToSelected(action: string): void {
    const binding = this.getSelectedBinding()
    if (!binding) return

    binding.action = action
    binding.label = this.getActionLabel(action)
    binding.enabled = true
    const runtimeHotkey = this.getRuntimeHotkeyForBinding(binding)

    const currentHotkey = this.container.querySelector('#ots-keypad-current-hotkey') as HTMLElement | null
    if (currentHotkey) {
      currentHotkey.textContent = this.formatHotkey(runtimeHotkey)
    }

    const title = this.container.querySelector('#ots-keypad-selected-title') as HTMLElement | null
    if (title) {
      title.textContent = `K${binding.keyId} · ${binding.label ?? binding.action}`
    }

    this.updateLayoutVisuals()
    this.updateValidation()
  }

  private formatHotkey(hotkey: string | null): string {
    if (!hotkey) return '-'
    if (hotkey === ' ') return 'Space'
    return hotkey ? hotkey.toUpperCase() : '-'
  }

  private getBridgeHotkeysOrDefault(): Record<string, string> {
    const bridgeHotkeys = ((window as any).otsGameBridge?.getCurrentKeybindHotkeys?.() as Record<string, string> | undefined)
    if (bridgeHotkeys && Object.keys(bridgeHotkeys).length > 0) {
      return bridgeHotkeys
    }

    return DEFAULT_GAME_HOTKEYS
  }

  private getBridgeKeybindCodesOrDefault(): Record<string, string> {
    const bridgeCodes = ((window as any).otsGameBridge?.getCurrentKeybinds?.() as Record<string, string> | undefined)
    if (bridgeCodes && Object.keys(bridgeCodes).length > 0) {
      return bridgeCodes
    }

    return DEFAULT_GAME_KEYBIND_CODES
  }

  private renderOpenFrontKeybindOptions(): string {
    const keybindCodes = this.getBridgeKeybindCodesOrDefault()
    const keybindHotkeys = this.getBridgeHotkeysOrDefault()
    const actions = Object.keys(keybindCodes).sort((a, b) => {
      const labelA = GAME_KEYBIND_LABELS[a] ?? this.humanizeAction(a)
      const labelB = GAME_KEYBIND_LABELS[b] ?? this.humanizeAction(b)
      return labelA.localeCompare(labelB)
    })

    const options = [
      '<option value="__DISABLED__">Disabled</option>',
      ...actions.map((action) => {
        const code = keybindCodes[action]
        const hotkey = keybindHotkeys[action] ?? this.keyCodeToHotkey(code)
        const label = GAME_KEYBIND_LABELS[action] ?? this.humanizeAction(action)
        const renderedHotkey = hotkey ? this.formatHotkey(hotkey) : 'Unbound'
        return `<option value="${action}">${label} (${renderedHotkey})</option>`
      })
    ]

    if (options.length === 0) {
      return '<option value="">No game keybinds detected</option>'
    }

    return options.join('')
  }

  private resetSelectedKeyToDefault(): void {
    if (this.selectedKeyId === null) return
    const defaults = getDefaultKeypadConfig().bindings
    const defaultBinding = defaults.find((binding) => binding.keyId === this.selectedKeyId)
    if (!defaultBinding) return

    const index = this.bindings.findIndex((binding) => binding.keyId === this.selectedKeyId)
    if (index === -1) return

    this.bindings[index] = { ...defaultBinding }
    this.selectKey(this.selectedKeyId)
    this.logInfo(`K${this.selectedKeyId} reset to default`)
  }

  private triggerBinding(binding: KeyBinding): void {
    if (!binding.enabled) {
      this.logInfo(`K${binding.keyId} is disabled`)
      return
    }

    const key = this.getRuntimeHotkeyForBinding(binding)
    if (!key) {
      this.logInfo(`Test K${binding.keyId}: action has no current game hotkey`)
      return
    }

    const code = key === ' '
      ? 'Space'
      : key.length === 1 && key >= 'a' && key <= 'z'
        ? `Key${key.toUpperCase()}`
        : key.length === 1 && key >= '0' && key <= '9'
          ? `Digit${key}`
          : key

    const down = new KeyboardEvent('keydown', { key, code, bubbles: true, cancelable: true })
    const up = new KeyboardEvent('keyup', { key, code, bubbles: true, cancelable: true })
    window.dispatchEvent(down)
    window.dispatchEvent(up)
    this.logInfo(`Test K${binding.keyId}: sent hotkey ${key === ' ' ? 'Space' : key}`)
  }

  private getActionLabel(action: KeypadAction): string {
    return GAME_KEYBIND_LABELS[action] ?? this.humanizeAction(action)
  }

  private getRuntimeHotkeyForAction(action: KeypadAction): string | null {
    const bridge = (window as any).otsGameBridge as {
      resolveHotkeyForKeypadAction?: (value: KeypadAction) => string | null
    } | undefined

    const runtime = bridge?.resolveHotkeyForKeypadAction?.(action)
    if (runtime) {
      return runtime
    }

    const fallback = this.getBridgeHotkeysOrDefault()[action]
    return fallback ?? null
  }

  private getRuntimeHotkeyForBinding(binding: KeyBinding): string | null {
    return this.getRuntimeHotkeyForAction(binding.action)
  }

  handleLiveKeyEvent(data: unknown): void {
    if (!this.isKeypadKeyEventData(data)) {
      return
    }

    if (data.state === 'pressed') {
      this.livePressedKeys.add(data.keyId)
    } else {
      this.livePressedKeys.delete(data.keyId)
    }

    this.updateLayoutVisuals()
  }

  private isKeypadKeyEventData(value: unknown): value is KeypadKeyEventData {
    if (!value || typeof value !== 'object') return false
    const data = value as Record<string, unknown>
    return typeof data.keyId === 'number' && (data.state === 'pressed' || data.state === 'released')
  }

  private keyCodeToHotkey(code: string): string | null {
    if (!code || code === 'Null') return null
    if (code === 'Space' || code === 'Spacebar') return ' '

    const digitMatch = /^Digit([0-9])$/.exec(code)
    if (digitMatch) return digitMatch[1]

    const keyMatch = /^Key([A-Z])$/.exec(code)
    if (keyMatch) return keyMatch[1].toLowerCase()

    if (code.length === 1) return code.toLowerCase()
    return null
  }

  private humanizeAction(action: string): string {
    return action
      .replace(/([A-Z])/g, ' $1')
      .replace(/_/g, ' ')
      .replace(/^./, (char) => char.toUpperCase())
      .trim()
  }

}
