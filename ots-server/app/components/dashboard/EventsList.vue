<template>
  <UCard>
    <template #header>
      <h2 class="text-sm font-semibold text-slate-300">Recent Events</h2>
    </template>

    <div class="space-y-2 max-h-112 overflow-auto text-xs">
      <article
        v-for="event in events"
        :key="`${event.timestamp}-${event.message}`"
        class="rounded border border-dashed border-slate-800/60 bg-slate-950/40 px-3 py-2"
      >
        <header class="mb-1 flex items-center justify-between gap-2">
          <span
            class="inline-flex items-center rounded px-1.5 py-0.5 text-[10px] font-semibold uppercase tracking-wide"
            :class="eventBadgeClass(event.type)"
          >
            {{ event.type }}
          </span>
          <span class="font-mono text-[10px] text-slate-500">
            {{ new Date(event.timestamp).toLocaleTimeString() }}
          </span>
        </header>
        <p class="text-slate-200">
          <template
            v-if="
              event.message === 'ui-cmd-sent' &&
              event.data &&
              typeof event.data === 'object' &&
              !Array.isArray(event.data) &&
              'action' in event.data
            "
          >
            Command sent:
            <span class="font-mono text-slate-100">
              {{ (event.data as any).action }}
            </span>
          </template>
          <template v-else>
            {{ event.message }}
          </template>
        </p>
      </article>
    </div>
    <p v-if="!events.length" class="mt-2 text-xs text-slate-500">
      No events yet. They will appear here as they arrive.
    </p>
  </UCard>
</template>

<script setup lang="ts">
import type { GameEvent, GameEventType } from '~/../../ots-shared/src/game'

const props = defineProps<{
  events: GameEvent[]
}>()

const { events } = toRefs(props)

const eventBadgeClass = (type: GameEventType) => {
  switch (type) {
    case 'KILL':
      return 'bg-emerald-500/15 text-emerald-300'
    case 'DEATH':
      return 'bg-rose-500/15 text-rose-300'
    case 'OBJECTIVE':
      return 'bg-sky-500/15 text-sky-300'
    case 'ERROR':
      return 'bg-rose-600/20 text-rose-300'
    case 'INFO':
    default:
      return 'bg-slate-700/40 text-slate-200'
  }
}
</script>
