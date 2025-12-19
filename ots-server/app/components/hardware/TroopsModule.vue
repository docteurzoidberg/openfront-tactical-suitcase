<template>
  <UCard>
    <template #header>
      <h2 class="text-sm font-semibold text-slate-300">Troops Module</h2>
    </template>

    <div class="flex flex-col gap-4">
      <!-- LCD Display Simulation (1602A Yellow-Green) -->
      <div class="flex justify-center">
        <div class="lcd-container">
          <div class="lcd-screen">
            <div class="lcd-content">
              <!-- Line 1: current / max -->
              <div class="lcd-line">{{ displayLine1 }}</div>
              <!-- Line 2: percent (calculated) -->
              <div class="lcd-line">{{ displayLine2 }}</div>
            </div>
          </div>
        </div>
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
}

const props = defineProps<Props>()
const emit = defineEmits<{
  (e: 'set-troops-percent', percent: number): void
}>()

const sliderValue = ref(0)
const isEnabled = computed(() => props.connected && props.powered)

// Format numbers with K/M/B suffix and 1 decimal place
function formatTroops(value: number): string {
  if (value >= 1_000_000_000) {
    return (value / 1_000_000_000).toFixed(1) + 'B'
  } else if (value >= 1_000_000) {
    return (value / 1_000_000).toFixed(1) + 'M'
  } else if (value >= 1_000) {
    return (value / 1_000).toFixed(1) + 'K'
  }
  return value.toString()
}

// Display line 1: "current / max"
const displayLine1 = computed(() => {
  if (!props.troops) {
    return '---  / ---     '
  }
  const current = formatTroops(props.troops.current)
  const max = formatTroops(props.troops.max)
  return `${current} / ${max}`.padEnd(16, ' ')
})

// Display line 2: "percent% (calculated)"
const displayLine2 = computed(() => {
  if (!props.troops) {
    return '0% (---)       '
  }
  const calculated = Math.floor((props.troops.current * sliderValue.value) / 100)
  const calculatedStr = formatTroops(calculated)
  return `${sliderValue.value}% (${calculatedStr})`.padEnd(16, ' ')
})

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
/* LCD 1602A Yellow-Green Display Styling */
.lcd-container {
  display: inline-block;
  padding: 12px;
  background: linear-gradient(135deg, #2c3e50 0%, #34495e 100%);
  border-radius: 8px;
  box-shadow: 
    inset 0 2px 4px rgba(0, 0, 0, 0.4),
    0 4px 8px rgba(0, 0, 0, 0.3);
}

.lcd-screen {
  background: linear-gradient(180deg, #9db86d 0%, #a8c470 50%, #9db86d 100%);
  border: 2px solid #4a5c3a;
  border-radius: 4px;
  padding: 8px 12px;
  box-shadow: 
    inset 0 1px 3px rgba(0, 0, 0, 0.3),
    inset 0 -1px 2px rgba(255, 255, 255, 0.1);
  position: relative;
}

.lcd-screen::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: linear-gradient(
    180deg,
    rgba(255, 255, 255, 0.1) 0%,
    transparent 50%,
    rgba(0, 0, 0, 0.05) 100%
  );
  pointer-events: none;
  border-radius: 2px;
}

.lcd-content {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.lcd-line {
  font-family: 'LCD Matrix', 'Courier New', monospace;
  font-size: 18px;
  font-weight: 400;
  line-height: 1.3;
  letter-spacing: 0.5px;
  color: #2d3d1f;
  text-shadow: 0 0 2px rgba(45, 61, 31, 0.5);
  white-space: pre;
  font-feature-settings: 'tnum';
}

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
