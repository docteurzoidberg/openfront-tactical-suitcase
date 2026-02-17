import type { KeyBinding, KeypadConfig } from '../types/keypad-types'
import { STORAGE_KEYS } from './keys'

const KEYPAD_CONFIG_VERSION = 1

const LEGACY_KEYPAD_ACTION_TO_GAME_ACTION: Record<string, string> = {
  BUILD_CITY: 'buildCity',
  BUILD_FACTORY: 'buildFactory',
  BUILD_PORT: 'buildPort',
  BUILD_DEFENSE: 'buildDefensePost',
  BUILD_MISSILE: 'buildMissileSilo',
  BUILD_SAM: 'buildSamLauncher',
  BUILD_WARSHIP: 'buildWarship',
  ZOOM_IN: 'zoomIn',
  ZOOM_OUT: 'zoomOut',
  ATTACK_DECREASE: 'attackRatioDown',
  MISSILE_SWITCH: 'swapDirection',
  ATTACK_INCREASE: 'attackRatioUp',
  BOAT_ATTACK: 'boatAttack',
  LAND_ATTACK: 'groundAttack',
  TOGGLE_VIEW: 'toggleView'
}

const DEFAULT_BINDINGS: KeyBinding[] = [
  { keyId: 1, action: 'buildCity', label: 'City', enabled: true },
  { keyId: 2, action: 'buildFactory', label: 'Factory', enabled: true },
  { keyId: 3, action: 'buildPort', label: 'Port', enabled: true },
  { keyId: 4, action: 'buildDefensePost', label: 'Defense', enabled: true },
  { keyId: 5, action: 'buildMissileSilo', label: 'Missile', enabled: true },
  { keyId: 6, action: 'buildSamLauncher', label: 'SAM', enabled: true },
  { keyId: 7, action: 'buildWarship', label: 'Warship', enabled: true },
  { keyId: 8, action: 'zoomIn', label: 'Zoom+', enabled: true },
  { keyId: 9, action: 'zoomOut', label: 'Zoom-', enabled: true },
  { keyId: 10, action: 'attackRatioDown', label: 'Atk-', enabled: true },
  { keyId: 11, action: 'swapDirection', label: 'Switch', enabled: true },
  { keyId: 12, action: 'attackRatioUp', label: 'Atk+', enabled: true },
  { keyId: 13, action: 'boatAttack', label: 'Naval', enabled: true },
  { keyId: 14, action: 'groundAttack', label: 'Land', enabled: true },
  { keyId: 15, action: 'toggleView', label: 'View', enabled: true }
]

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null
}

function isBinding(value: unknown): value is KeyBinding {
  if (!isRecord(value)) return false
  return (
    typeof value.keyId === 'number' &&
    typeof value.action === 'string' &&
    typeof value.label === 'string' &&
    typeof value.enabled === 'boolean'
  )
}

function isConfig(value: unknown): value is KeypadConfig {
  if (!isRecord(value)) return false
  if (typeof value.version !== 'number' || !Array.isArray(value.bindings)) return false
  return value.bindings.every(isBinding)
}

function normalizeBindingAction(binding: KeyBinding): KeyBinding {
  const migratedAction = LEGACY_KEYPAD_ACTION_TO_GAME_ACTION[binding.action] ?? binding.action
  if (migratedAction === binding.action) {
    return binding
  }

  return {
    ...binding,
    action: migratedAction
  }
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
    const normalizedBindings = saved.bindings.map(normalizeBindingAction)
    const changed = normalizedBindings.some((binding, index) => binding.action !== saved.bindings[index].action)
    if (changed) {
      const migrated: KeypadConfig = {
        ...saved,
        bindings: normalizedBindings
      }
      GM_setValue(STORAGE_KEYS.KEYPAD_BINDINGS, migrated)
      return migrated
    }

    return saved
  }

  const defaults = getDefaultKeypadConfig()
  GM_setValue(STORAGE_KEYS.KEYPAD_BINDINGS, defaults)
  return defaults
}

export function saveKeypadConfig(config: KeypadConfig): void {
  GM_setValue(STORAGE_KEYS.KEYPAD_BINDINGS, config)
}
