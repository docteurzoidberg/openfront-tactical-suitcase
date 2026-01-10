<template>
  <UCard>
    <template #header>
      <h2 class="text-sm font-semibold text-slate-300">Sound Module</h2>
    </template>

    <div class="flex flex-col gap-4">
      <div class="flex items-center justify-between gap-6 px-4">
        <!-- Power LED Indicator (Left) -->
        <div class="flex flex-col items-center gap-2">
          <div
            :class="[
              'h-8 w-8 rounded-full border-2 transition-all',
              modulePowered
                ? 'border-red-400 bg-red-500 shadow-lg shadow-red-500/80'
                : 'border-slate-600 bg-slate-800'
            ]"
          />
          <span class="text-[10px] font-semibold uppercase tracking-wide text-slate-400">Power</span>
        </div>

        <!-- On/Off Toggle (Center) -->
        <div class="flex flex-col items-center gap-2">
          <button
            :disabled="!powered"
            :class="[
              'relative h-16 w-32 rounded-lg border-2 transition-all',
              powered ? 'hover:scale-105 active:scale-95' : 'cursor-not-allowed opacity-50',
              modulePowered
                ? 'border-green-600 bg-green-950/30 hover:bg-green-900/40'
                : 'border-slate-600 bg-slate-900/30 hover:bg-slate-800/40'
            ]"
            @click="powered && emitToggle()"
          >
            <div class="flex h-full flex-col items-center justify-center">
              <div
                :class="[
                  'mb-1 h-6 w-12 rounded-full border-2 transition-all relative',
                  modulePowered
                    ? 'border-green-500 bg-green-600'
                    : 'border-slate-500 bg-slate-700'
                ]"
              >
                <div
                  :class="[
                    'absolute top-0.5 h-4 w-4 rounded-full bg-white transition-all duration-300',
                    modulePowered ? 'right-0.5' : 'left-0.5'
                  ]"
                />
              </div>
              <span
                :class="[
                  'text-xs font-bold uppercase transition-colors',
                  modulePowered ? 'text-green-400' : 'text-slate-400'
                ]"
              >
                {{ modulePowered ? 'ON' : 'OFF' }}
              </span>
            </div>
          </button>
          <span class="text-[10px] font-semibold uppercase tracking-wide text-slate-400">On/Off</span>
        </div>

        <!-- Volume Knob (Right) -->
        <div class="flex flex-col items-center gap-2">
          <div
            ref="knobElement"
            :class="[
              'relative flex h-16 w-16 cursor-pointer items-center justify-center rounded-full border-4 border-slate-700 bg-linear-to-br from-slate-800 to-slate-900 shadow-lg transition-all hover:border-slate-600'
            ]"
            @mousedown="handleMouseDown"
          >
            <!-- Knob position indicator -->
            <div
              class="absolute inset-0 transition-transform"
              :style="{ transform: `rotate(${knobRotation}deg)` }"
            >
              <div class="absolute left-1/2 top-2 h-3 w-1 -translate-x-1/2 rounded-full bg-cyan-400 shadow-lg shadow-cyan-500/50"></div>
            </div>
            
            <!-- Center dot -->
            <div class="h-4 w-4 rounded-full bg-slate-950"></div>
          </div>
          <span class="text-[10px] font-semibold uppercase tracking-wide text-slate-400">
            Volume {{ volume }}%
          </span>
        </div>
      </div>
    </div>
  </UCard>
</template>

<script setup lang="ts">
interface SoundPlaybackState {
  soundId: string
  requestedAt: number
  status: 'idle' | 'requested' | 'playing' | 'blocked' | 'missing' | 'powered-off'
  fileUrl?: string
  error?: string
}

interface Props {
  connected: boolean
  powered: boolean
  modulePowerOn: boolean
  volume: number
  lastSound: SoundPlaybackState | null
}

const props = defineProps<Props>()

const emit = defineEmits<{
  (e: 'toggle-module-power'): void
  (e: 'set-volume', volume: number): void
}>()

const modulePowered = computed(() => props.powered && props.modulePowerOn)
const isEnabled = computed(() => props.connected && modulePowered.value)

const emitToggle = () => {
  emit('toggle-module-power')
}

// Virtual knob control
const knobElement = ref<HTMLElement | null>(null)
const isDragging = ref(false)

// Map volume (0-100) to rotation angle (-135 to +135 degrees, 270° total range)
const knobRotation = computed(() => {
  const normalized = props.volume / 100 // 0 to 1
  return -135 + (normalized * 270) // -135° to +135°
})

const handleMouseDown = (event: MouseEvent) => {
  startDrag(event)
}

const startDrag = (event: MouseEvent) => {
  event.preventDefault()
  isDragging.value = true
  updateKnobFromMouse(event)
  
  const onMouseMove = (e: MouseEvent) => {
    e.preventDefault()
    if (isDragging.value) {
      updateKnobFromMouse(e)
    }
  }
  
  const onMouseUp = () => {
    isDragging.value = false
    document.removeEventListener('mousemove', onMouseMove)
    document.removeEventListener('mouseup', onMouseUp)
  }
  
  document.addEventListener('mousemove', onMouseMove)
  document.addEventListener('mouseup', onMouseUp)
}

const updateKnobFromMouse = (event: MouseEvent) => {
  if (!knobElement.value) return
  
  const rect = knobElement.value.getBoundingClientRect()
  const centerX = rect.left + rect.width / 2
  const centerY = rect.top + rect.height / 2
  
  // Calculate angle from center to mouse
  const dx = event.clientX - centerX
  const dy = event.clientY - centerY
  let angle = Math.atan2(dy, dx) * (180 / Math.PI)
  
  // Convert to our coordinate system (0° = top, clockwise)
  angle = angle + 90
  
  // Normalize to -180° to +180° range first
  while (angle > 180) angle -= 360
  while (angle < -180) angle += 360
  
  // Clamp to valid range (-135° to +135°)
  angle = Math.max(-135, Math.min(135, angle))
  
  // Convert angle to volume (0-100)
  const volume = Math.round(((angle + 135) / 270) * 100)
  emit('set-volume', Math.max(0, Math.min(100, volume)))
}
</script>
