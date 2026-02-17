import type { KeyBinding, KeypadConfig } from '../types/keypad-types'
import { STORAGE_KEYS } from './keys'

const KEYPAD_CONFIG_VERSION = 1

const DEFAULT_BINDINGS: KeyBinding[] = [
  { keyId: 1, action: 'BUILD_CITY', selector: '[data-hotkey="1"]', label: 'City', enabled: true, hotkey: '1' },
  { keyId: 2, action: 'BUILD_FACTORY', selector: '[data-hotkey="2"]', label: 'Factory', enabled: true, hotkey: '2' },
  { keyId: 3, action: 'BUILD_PORT', selector: '[data-hotkey="3"]', label: 'Port', enabled: true, hotkey: '3' },
  { keyId: 4, action: 'BUILD_DEFENSE', selector: '[data-hotkey="4"]', label: 'Defense', enabled: true, hotkey: '4' },
  { keyId: 5, action: 'BUILD_MISSILE', selector: '[data-hotkey="5"]', label: 'Missile', enabled: true, hotkey: '5' },
  { keyId: 6, action: 'BUILD_SAM', selector: '[data-hotkey="6"]', label: 'SAM', enabled: true, hotkey: '6' },
  { keyId: 7, action: 'BUILD_WARSHIP', selector: '[data-hotkey="7"]', label: 'Warship', enabled: true, hotkey: '7' },
  { keyId: 8, action: 'ZOOM_IN', selector: '[data-hotkey="e"]', label: 'Zoom+', enabled: true, hotkey: 'e' },
  { keyId: 9, action: 'ZOOM_OUT', selector: '[data-hotkey="q"]', label: 'Zoom-', enabled: true, hotkey: 'q' },
  { keyId: 10, action: 'ATTACK_DECREASE', selector: '[data-hotkey="t"]', label: 'Atk-', enabled: true, hotkey: 't' },
  { keyId: 11, action: 'MISSILE_SWITCH', selector: '[data-hotkey="u"]', label: 'Switch', enabled: true, hotkey: 'u' },
  { keyId: 12, action: 'ATTACK_INCREASE', selector: '[data-hotkey="y"]', label: 'Atk+', enabled: true, hotkey: 'y' },
  { keyId: 13, action: 'BOAT_ATTACK', selector: '[data-hotkey="b"]', label: 'Naval', enabled: true, hotkey: 'b' },
  { keyId: 14, action: 'LAND_ATTACK', selector: '[data-hotkey="g"]', label: 'Land', enabled: true, hotkey: 'g' },
  { keyId: 15, action: 'TOGGLE_VIEW', selector: '[data-hotkey=" "]', label: 'View', enabled: true, hotkey: ' ' }
]

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null
}

function isBinding(value: unknown): value is KeyBinding {
  if (!isRecord(value)) return false
  return (
    typeof value.keyId === 'number' &&
    typeof value.action === 'string' &&
    typeof value.selector === 'string' &&
    typeof value.label === 'string' &&
    typeof value.enabled === 'boolean' &&
    typeof value.hotkey === 'string'
  )
}

function isConfig(value: unknown): value is KeypadConfig {
  if (!isRecord(value)) return false
  if (typeof value.version !== 'number' || !Array.isArray(value.bindings)) return false
  return value.bindings.every(isBinding)
}

export function getDefaultKeypadConfig(): KeypadConfig {
  return {
    version: KEYPAD_CONFIG_VERSION,
    bindings: DEFAULT_BINDINGS
  }
}

export function loadKeypadConfig(): KeypadConfig {
  const saved = GM_getValue<unknown>(STORAGE_KEYS.KEYPAD_BINDINGS, null)

  if (isConfig(saved) && saved.version === KEYPAD_CONFIG_VERSION) {
    return saved
  }

  const defaults = getDefaultKeypadConfig()
  GM_setValue(STORAGE_KEYS.KEYPAD_BINDINGS, defaults)
  return defaults
}

export function saveKeypadConfig(config: KeypadConfig): void {
  GM_setValue(STORAGE_KEYS.KEYPAD_BINDINGS, config)
}
