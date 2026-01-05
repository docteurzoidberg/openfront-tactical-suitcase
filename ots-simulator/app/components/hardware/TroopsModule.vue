<template>
  <UCard>
    <template #header>
      <h2 class="text-sm font-semibold text-slate-300">Troops Module</h2>
    </template>

    <div class="flex flex-col gap-4">
      <!-- LCD Display - Shows all system phases -->
      <div class="flex justify-center">
        <HardwareLCDDisplay
          :powered="powered"
          :connected="connected"
          :game-phase="gamePhase"
          :troops="troops"
          :slider-percent="sliderValue"
        />
      </div>

      <!-- Slider Control -->
      <div class="flex flex-col gap-2">
        <label class="text-xs font-semibold text-slate-400">
          Deployment Percentage: {{ sliderValue }}%
        </label>
        <input
          type="range"
          :value="sliderValue"
          :disabled="!isEnabled"
          min="0"
          max="100"
          step="1"
          :class="[
            'h-2 w-full cursor-pointer appearance-none rounded-lg bg-slate-700',
            !isEnabled && 'cursor-not-allowed opacity-50'
          ]"
          @input="handleSliderChange"
        />
        <div class="flex justify-between text-xs text-slate-500">
          <span>0%</span>
          <span>50%</span>
          <span>100%</span>
        </div>
      </div>
    </div>
  </UCard>
</template>

<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import type { TroopsData } from '../../../../ots-shared/src/game'

interface Props {
  connected: boolean
  powered: boolean
  troops: TroopsData | null
  attackRatio?: number | null  // Attack ratio from game (0-100)
  gamePhase?: any | null       // Game phase for LCD display
}

const props = defineProps<Props>()
const emit = defineEmits<{
  (e: 'set-troops-percent', percent: number): void
}>()

const sliderValue = ref(0)
const isEnabled = computed(() => props.connected && props.powered && props.gamePhase === 'in-game')

// Handle slider change (debounced to avoid spam)
let debounceTimer: NodeJS.Timeout | null = null
function handleSliderChange(event: Event) {
  const target = event.target as HTMLInputElement
  const newValue = parseInt(target.value, 10)
  
  // Update local state immediately for responsive UI
  sliderValue.value = newValue

  // Debounce command sending
  if (debounceTimer) {
    clearTimeout(debounceTimer)
  }
  
  debounceTimer = setTimeout(() => {
    emit('set-troops-percent', newValue)
  }, 100)
}

// Sync slider with attack ratio from game
watch(() => props.attackRatio, (newRatio) => {
  if (newRatio !== null && newRatio !== undefined) {
    const rounded = Math.round(newRatio)
    // Only update if different to avoid unnecessary updates
    if (sliderValue.value !== rounded) {
      sliderValue.value = rounded
      console.log('[TroopsModule] Synced slider with game ratio:', rounded)
    }
  }
}, { immediate: true })

// Reset slider when troops data becomes available
watch(() => props.troops, (newTroops) => {
  if (newTroops && sliderValue.value === 0 && !props.attackRatio) {
    // Could optionally set to a default value
  }
})
</script>

<style scoped>
/* Custom range slider styling */
input[type='range']::-webkit-slider-thumb {
  appearance: none;
  width: 20px;
  height: 20px;
  border-radius: 50%;
  background: #22d3ee;
  cursor: pointer;
  transition: all 0.15s ease-in-out;
}

input[type='range']::-webkit-slider-thumb:hover {
  background: #06b6d4;
  transform: scale(1.1);
}

input[type='range']::-moz-range-thumb {
  width: 20px;
  height: 20px;
  border: none;
  border-radius: 50%;
  background: #22d3ee;
  cursor: pointer;
  transition: all 0.15s ease-in-out;
}

input[type='range']::-moz-range-thumb:hover {
  background: #06b6d4;
  transform: scale(1.1);
}

input[type='range']:disabled::-webkit-slider-thumb {
  background: #475569;
  cursor: not-allowed;
}

input[type='range']:disabled::-moz-range-thumb {
  background: #475569;
  cursor: not-allowed;
}
</style>
