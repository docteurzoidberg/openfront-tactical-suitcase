<template>
  <div class="flex items-center justify-between gap-4">
    <div>
      <h1 class="text-2xl font-semibold tracking-tight">Openfront Tactical Suitcase emulator</h1>
      <p class="text-sm text-slate-400">Real-time state and events from the userscript</p>
    </div>
    <div class="flex flex-wrap items-center gap-4 text-sm">
      <div class="flex items-center gap-2">
        <span
          class="inline-flex h-2 w-2 rounded-full"
          :class="{
            'bg-emerald-400': uiStatus === 'OPEN',
            'bg-amber-400': uiStatus === 'CONNECTING',
            'bg-rose-500': uiStatus === 'CLOSED'
          }"
        />
        <span class="uppercase tracking-wide text-xs text-slate-400">UI WS: {{ uiStatus }}</span>
      </div>
      <div class="flex items-center gap-2">
        <span
          class="inline-flex h-2 w-2 rounded-full"
          :class="{
            'bg-emerald-400': userscriptStatus === 'ONLINE',
            'bg-amber-400': userscriptStatus === 'STALE',
            'bg-slate-500': userscriptStatus === 'UNKNOWN',
            'bg-rose-500': userscriptStatus === 'OFFLINE',
            'animate-ping-once': userscriptBlink
          }"
        />
        <span class="uppercase tracking-wide text-xs text-slate-400">USERSCRIPT: {{ userscriptStatus }}</span>
      </div>
      <div class="flex items-center gap-2">
        <span
          class="inline-flex h-2 w-2 rounded-full"
          :class="{
            'bg-emerald-400': gameStatus === 'IN_GAME',
            'bg-amber-400': gameStatus === 'SPAWNING',
            'bg-slate-500': gameStatus === 'LOBBY',
            'bg-emerald-500': gameStatus === 'WON',
            'bg-rose-500': gameStatus === 'LOST',
            'bg-slate-600': gameStatus === 'UNKNOWN'
          }"
        />
        <span class="uppercase tracking-wide text-xs text-slate-400">GAME: {{ gameStatus }}</span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
const props = defineProps<{
  uiStatus: string
  userscriptStatus: string
  userscriptBlink: boolean
  gameStatus: string
}>()

const { uiStatus, userscriptStatus, userscriptBlink, gameStatus } = toRefs(props)
</script>
