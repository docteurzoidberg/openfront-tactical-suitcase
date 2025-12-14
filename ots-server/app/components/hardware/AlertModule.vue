<template>
  <UCard>
    <template #header>
      <h2 class="text-sm font-semibold text-slate-300">Alert Module</h2>
    </template>

    <div :class="['grid grid-cols-3 gap-6 px-4 py-2 transition-opacity', !powered && 'opacity-50']">
      <!-- Column 1: Warning LED (Large, centered) -->
      <div class="flex flex-col items-center justify-center">
        <div
          :class="[
            'h-16 w-16 rounded-full border-4 transition-all',
            activeAlerts.warning
              ? 'animate-pulse border-red-400 bg-red-500 shadow-lg shadow-red-500/80'
              : 'border-slate-600 bg-slate-800'
          ]"
        />
        <span class="mt-3 text-xs font-semibold uppercase tracking-wide text-slate-400">
          Warning
        </span>
      </div>

      <!-- Column 2: Nuke Alerts (ATOM, HYDRO, MIRV) -->
      <div class="flex flex-col justify-center gap-3">
        <!-- ATOM Alert -->
        <div class="flex items-center gap-3">
          <div
            :class="[
              'h-8 w-8 rounded-full border-2 transition-all',
              activeAlerts.atom
                ? 'animate-pulse border-red-400 bg-red-500 shadow-lg shadow-red-500/80'
                : 'border-slate-600 bg-slate-800'
            ]"
          />
          <span class="text-xs font-semibold uppercase tracking-wide text-slate-400">
            Atom
          </span>
        </div>

        <!-- HYDRO Alert -->
        <div class="flex items-center gap-3">
          <div
            :class="[
              'h-8 w-8 rounded-full border-2 transition-all',
              activeAlerts.hydro
                ? 'animate-pulse border-red-400 bg-red-500 shadow-lg shadow-red-500/80'
                : 'border-slate-600 bg-slate-800'
            ]"
          />
          <span class="text-xs font-semibold uppercase tracking-wide text-slate-400">
            Hydro
          </span>
        </div>

        <!-- MIRV Alert -->
        <div class="flex items-center gap-3">
          <div
            :class="[
              'h-8 w-8 rounded-full border-2 transition-all',
              activeAlerts.mirv
                ? 'animate-pulse border-red-400 bg-red-500 shadow-lg shadow-red-500/80'
                : 'border-slate-600 bg-slate-800'
            ]"
          />
          <span class="text-xs font-semibold uppercase tracking-wide text-slate-400">
            MIRV
          </span>
        </div>
      </div>

      <!-- Column 3: Invasion Alerts (LAND, NAVAL) -->
      <div class="flex flex-col justify-center gap-3">
        <!-- LAND Alert -->
        <div class="flex items-center gap-3">
          <div
            :class="[
              'h-8 w-8 rounded-full border-2 transition-all',
              activeAlerts.land
                ? 'animate-pulse border-red-400 bg-red-500 shadow-lg shadow-red-500/80'
                : 'border-slate-600 bg-slate-800'
            ]"
          />
          <span class="text-xs font-semibold uppercase tracking-wide text-slate-400">
            Land
          </span>
        </div>

        <!-- NAVAL Alert -->
        <div class="flex items-center gap-3">
          <div
            :class="[
              'h-8 w-8 rounded-full border-2 transition-all',
              activeAlerts.naval
                ? 'animate-pulse border-red-400 bg-red-500 shadow-lg shadow-red-500/80'
                : 'border-slate-600 bg-slate-800'
            ]"
          />
          <span class="text-xs font-semibold uppercase tracking-wide text-slate-400">
            Naval
          </span>
        </div>
      </div>
    </div>
  </UCard>
</template>

<script setup lang="ts">
interface Props {
  connected: boolean
  powered: boolean
  activeAlerts: {
    warning: boolean
    atom: boolean
    hydro: boolean
    mirv: boolean
    land: boolean
    naval: boolean
  }
}

defineProps<Props>()
</script>
