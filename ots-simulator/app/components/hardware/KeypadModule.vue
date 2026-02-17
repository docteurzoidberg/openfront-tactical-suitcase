<template>
  <UCard>
    <template #header>
      <div class="flex items-center justify-between">
        <h2 class="text-sm font-semibold text-slate-300">Keypad Module</h2>
        <div class="flex items-center gap-2">
          <div
            :class="[
              'h-2 w-2 rounded-full transition-all',
              connected
                ? 'bg-green-500 shadow-lg shadow-green-500/80'
                : 'bg-slate-600'
            ]"
          />
          <span class="text-xs text-slate-400">
            {{ connected ? 'Connected' : 'Disconnected' }}
          </span>
          <span v-if="connected && firmwareVersion" class="text-xs text-slate-500">
            ({{ firmwareVersion }})
          </span>
        </div>
      </div>
    </template>

    <KeypadVisualizer
      :key-states="keyStates"
      :led-states="ledStates"
    />
  </UCard>
</template>

<script setup lang="ts">
import { useKeypad } from '~/composables/useKeypad'
import KeypadVisualizer from './KeypadVisualizer.vue'

const { keyStates, ledStates, connected, firmwareVersion } = useKeypad()
</script>
