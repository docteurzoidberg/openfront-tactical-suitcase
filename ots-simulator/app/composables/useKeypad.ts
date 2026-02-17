import { ref, readonly, computed } from 'vue'
import type { LedState } from '../types/keypad'
import { useGameSocket } from './useGameSocket'

export const useKeypad = () => {
  const { keypadConnected, keypadFirmwareVersion, keypadKeyStates } = useGameSocket()

  // LED states (future feature - currently all off)
  const ledStates = ref<Map<number, LedState>>(new Map())

  // Initialize LED states
  for (let i = 1; i <= 15; i++) {
    ledStates.value.set(i, { on: false, color: { r: 0, g: 0, b: 0 } })
  }

  return {
    keyStates: computed(() => keypadKeyStates.value),
    ledStates: readonly(ledStates),
    connected: computed(() => keypadConnected.value),
    firmwareVersion: computed(() => keypadFirmwareVersion.value)
  }
}
