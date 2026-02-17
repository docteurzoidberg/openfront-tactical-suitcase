import type { Hud } from '../hud/sidebar-hud'
import { loadKeypadConfig } from '../storage/keypad'
import type { KeyBinding, KeypadKeyEventData } from '../types/keypad-types'

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null
}

function parseKeypadEventData(data: unknown): KeypadKeyEventData | null {
  if (!isRecord(data)) return null
  if (typeof data.keyId !== 'number') return null
  if (data.state !== 'pressed' && data.state !== 'released') return null

  return {
    keyId: data.keyId,
    state: data.state,
    timestamp: typeof data.timestamp === 'number' ? data.timestamp : undefined
  }
}

function dispatchKey(key: string, type: 'keydown' | 'keyup') {
  const code = key === ' '
    ? 'Space'
    : key.length === 1 && key >= 'a' && key <= 'z'
      ? `Key${key.toUpperCase()}`
      : key.length === 1 && key >= '0' && key <= '9'
        ? `Digit${key}`
        : key

  const event = new KeyboardEvent(type, {
    key,
    code,
    bubbles: true,
    cancelable: true
  })
  document.dispatchEvent(event)
}

export class KeypadManager {
  private pressed = new Set<number>()

  constructor(private hud: Hud) { }

  handleKeyEvent(data: unknown): void {
    const parsed = parseKeypadEventData(data)
    if (!parsed) return

    const config = loadKeypadConfig()
    const binding = config.bindings.find((item) => item.keyId === parsed.keyId)
    if (!binding || !binding.enabled) return

    if (parsed.state === 'pressed') {
      if (this.pressed.has(parsed.keyId)) return
      this.pressed.add(parsed.keyId)
      this.triggerPressed(binding)
      return
    }

    this.pressed.delete(parsed.keyId)
    this.triggerReleased(binding)
  }

  private triggerPressed(binding: KeyBinding): void {
    const element = binding.selector ? (document.querySelector(binding.selector) as HTMLElement | null) : null

    if (element) {
      element.click()
      return
    }

    if (binding.hotkey) {
      dispatchKey(binding.hotkey, 'keydown')
      dispatchKey(binding.hotkey, 'keyup')
      return
    }

    this.hud.pushLog('info', `[KEYPAD] No selector/hotkey configured for K${binding.keyId}`)
  }

  private triggerReleased(binding: KeyBinding): void {
    if (binding.hotkey) {
      dispatchKey(binding.hotkey, 'keyup')
    }
  }
}
