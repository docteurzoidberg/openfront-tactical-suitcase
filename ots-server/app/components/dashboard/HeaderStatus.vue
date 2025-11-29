<template>
  <div class="flex items-center justify-between gap-4">
    <div>
      <h1 class="text-2xl font-semibold tracking-tight">Game Dashboard</h1>
      <p class="text-sm text-slate-400">Real-time state and events from the userscript</p>
    </div>
    <div class="flex flex-wrap items-center gap-4 text-sm">
      <div class="flex items-center gap-2">
        <span
          class="inline-flex h-2 w-2 rounded-full"
          :class="{
            'bg-emerald-400': status === 'OPEN',
            'bg-amber-400': status === 'CONNECTING',
            'bg-rose-500': status === 'CLOSED'
          }"
        />
        <span class="uppercase tracking-wide text-xs text-slate-400">UI WS: {{ status }}</span>
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
    </div>
  </div>
</template>

<script setup lang="ts">
const props = defineProps<{
  status: string
  userscriptStatus: string
  userscriptBlink: boolean
}>()

const { status, userscriptStatus, userscriptBlink } = toRefs(props)
</script>
