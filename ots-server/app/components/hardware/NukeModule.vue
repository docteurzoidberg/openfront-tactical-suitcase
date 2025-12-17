<template>
  <UCard>
    <template #header>
      <h2 class="text-sm font-semibold text-slate-300">Nuke Control Panel</h2>
    </template>

    <div class="flex flex-col gap-4">
      <div class="grid grid-cols-3 gap-4">
        <!-- Atom Nuke Button -->
        <button
          :disabled="!isEnabled"
          :class="[
            'relative flex flex-col items-center justify-center rounded-lg border-2 p-6 transition-all',
            isEnabled ? 'hover:scale-105 active:scale-95' : 'cursor-not-allowed',
            activeNukes.atom
              ? 'border-red-500 bg-red-900/50 shadow-lg shadow-red-500/50'
              : 'border-red-700 bg-red-950/30',
            isEnabled && !activeNukes.atom && 'hover:bg-red-900/40',
            !isEnabled && 'opacity-50'
          ]"
          @click="isEnabled && sendNuke('atom')"
        >
          <div
            :class="[
              'mb-2 h-12 w-12 rounded-full border-2 transition-all',
              activeNukes.atom
                ? 'animate-pulse border-red-400 bg-red-500 shadow-lg shadow-red-500/80'
                : 'border-slate-700 bg-slate-900'
            ]"
          />
          <span class="text-xs font-bold uppercase text-red-300">Atom</span>
        </button>

        <!-- Hydro Nuke Button -->
        <button
          :disabled="!isEnabled"
          :class="[
            'relative flex flex-col items-center justify-center rounded-lg border-2 p-6 transition-all',
            isEnabled ? 'hover:scale-105 active:scale-95' : 'cursor-not-allowed',
            activeNukes.hydro
              ? 'border-red-500 bg-red-900/50 shadow-lg shadow-red-500/50'
              : 'border-red-700 bg-red-950/30',
            isEnabled && !activeNukes.hydro && 'hover:bg-red-900/40',
            !isEnabled && 'opacity-50'
          ]"
          @click="isEnabled && sendNuke('hydro')"
        >
          <div
            :class="[
              'mb-2 h-12 w-12 rounded-full border-2 transition-all',
              activeNukes.hydro
                ? 'animate-pulse border-red-400 bg-red-500 shadow-lg shadow-red-500/80'
                : 'border-slate-700 bg-slate-900'
            ]"
          />
          <span class="text-xs font-bold uppercase text-red-300">Hydro</span>
        </button>

        <!-- MIRV Nuke Button -->
        <button
          :disabled="!isEnabled"
          :class="[
            'relative flex flex-col items-center justify-center rounded-lg border-2 p-6 transition-all',
            isEnabled ? 'hover:scale-105 active:scale-95' : 'cursor-not-allowed',
            activeNukes.mirv
              ? 'border-red-500 bg-red-900/50 shadow-lg shadow-red-500/50'
              : 'border-red-700 bg-red-950/30',
            isEnabled && !activeNukes.mirv && 'hover:bg-red-900/40',
            !isEnabled && 'opacity-50'
          ]"
          @click="isEnabled && sendNuke('mirv')"
        >
          <div
            :class="[
              'mb-2 h-12 w-12 rounded-full border-2 transition-all',
              activeNukes.mirv
                ? 'animate-pulse border-red-400 bg-red-500 shadow-lg shadow-red-500/80'
                : 'border-slate-700 bg-slate-900'
            ]"
          />
          <span class="text-xs font-bold uppercase text-red-300">MIRV</span>
        </button>
      </div>
    </div>
  </UCard>
</template>

<script setup lang="ts">
import type { NukeType } from '../../../../ots-shared/src/game'

interface Props {
  connected: boolean
  powered: boolean
  activeNukes: {
    atom: boolean
    hydro: boolean
    mirv: boolean
  }
}

interface Emits {
  (e: 'sendNuke', nukeType: NukeType): void
}

const props = defineProps<Props>()
const emit = defineEmits<Emits>()

const isEnabled = computed(() => props.connected && props.powered)

const sendNuke = (nukeType: NukeType) => {
  emit('sendNuke', nukeType)
}
</script>
