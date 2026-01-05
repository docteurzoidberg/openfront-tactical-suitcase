<template>
  <UCard>
    <template #header>
      <h2 class="text-sm font-semibold text-slate-300">Main Power Module</h2>
    </template>

    <div class="flex items-center justify-between gap-6 px-4">
      <!-- Power LED Indicator (Left) -->
      <div class="flex flex-col items-center gap-2">
        <div
          :class="[
            'h-8 w-8 rounded-full border-2 transition-all',
            powerOn
              ? 'border-red-400 bg-red-500 shadow-lg shadow-red-500/80'
              : 'border-slate-600 bg-slate-800'
          ]"
        />
        <span class="text-[10px] font-semibold uppercase tracking-wide text-slate-400">
          Power
        </span>
      </div>

      <!-- Power Toggle Switch (Center) -->
      <div class="flex flex-col items-center gap-2">
        <button
          :class="[
            'relative h-16 w-32 rounded-lg border-2 transition-all',
            'hover:scale-105 active:scale-95',
            powerOn
              ? 'border-green-600 bg-green-950/30 hover:bg-green-900/40'
              : 'border-slate-600 bg-slate-900/30 hover:bg-slate-800/40'
          ]"
          @click="togglePower"
        >
          <div class="flex h-full flex-col items-center justify-center">
            <div
              :class="[
                'mb-1 h-6 w-12 rounded-full border-2 transition-all relative',
                powerOn
                  ? 'border-green-500 bg-green-600'
                  : 'border-slate-500 bg-slate-700'
              ]"
            >
              <!-- Toggle knob -->
              <div
                :class="[
                  'absolute top-0.5 h-4 w-4 rounded-full bg-white transition-all duration-300',
                  powerOn ? 'right-0.5' : 'left-0.5'
                ]"
              />
            </div>
            <span
              :class="[
                'text-xs font-bold uppercase transition-colors',
                powerOn ? 'text-green-400' : 'text-slate-400'
              ]"
            >
              {{ powerOn ? 'ON' : 'OFF' }}
            </span>
          </div>
        </button>
        <span class="text-[10px] font-semibold uppercase tracking-wide text-slate-400">
          On/Off
        </span>
      </div>

      <!-- Link LED Indicator (Right) -->
      <div class="flex flex-col items-center gap-2">
        <div
          :class="[
            'h-8 w-8 rounded-full border-2 transition-all',
            linkActive
              ? 'border-red-400 bg-red-500 shadow-lg shadow-red-500/80'
              : 'border-slate-600 bg-slate-800'
          ]"
        />
        <span class="text-[10px] font-semibold uppercase tracking-wide text-slate-400">
          Link
        </span>
      </div>
    </div>
  </UCard>
</template>

<script setup lang="ts">
interface Props {
  connected: boolean
  powerOn: boolean
}

interface Emits {
  (e: 'togglePower'): void
}

const props = defineProps<Props>()
const emit = defineEmits<Emits>()

const linkActive = computed(() => props.connected && props.powerOn)

const togglePower = () => {
  emit('togglePower')
}
</script>
