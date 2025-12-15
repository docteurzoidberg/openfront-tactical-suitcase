<template>
  <UCard>
    <template #header>
      <div class="flex items-center justify-between">
        <h2 class="text-sm font-semibold text-slate-300">Recent Events</h2>
        <button
          @click="autoScroll = !autoScroll"
          :class="[
            'rounded px-2 py-1 text-xs font-medium transition-colors',
            autoScroll
              ? 'bg-emerald-500/20 text-emerald-300 hover:bg-emerald-500/30'
              : 'bg-slate-700/40 text-slate-400 hover:bg-slate-700/60'
          ]"
        >
          {{ autoScroll ? '✓ Auto-scroll' : 'Auto-scroll' }}
        </button>
      </div>
    </template>

    <div ref="scrollContainer" class="h-[calc(100vh-16rem)] overflow-y-auto pr-2">
      <div v-if="events.length" class="space-y-2 text-xs">
        <article
          v-for="(event, index) in reversedEvents"
          :key="`${event.timestamp}-${event.message}`"
          :class="[
            'rounded border border-dashed border-slate-800/60 bg-slate-950/40 px-3 py-2',
            index === reversedEvents.length - 1 && 'new-event-animation'
          ]"
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
          <template v-else-if="event.type === 'ALERT_ATOM' && event.data && typeof event.data === 'object'">
            <span class="font-semibold text-red-400">⚠️ ATOMIC BOMB INCOMING!</span>
            <br />
            <span class="text-slate-400">
              From: <span class="text-slate-200">{{ (event.data as any).launcherPlayerName || 'Unknown' }}</span>
              | Target: ({{ (event.data as any).coordinates?.x || '?' }}, {{ (event.data as any).coordinates?.y || '?' }})
            </span>
          </template>
          <template v-else-if="event.type === 'ALERT_HYDRO' && event.data && typeof event.data === 'object'">
            <span class="font-semibold text-red-400">⚠️ HYDROGEN BOMB INCOMING!</span>
            <br />
            <span class="text-slate-400">
              From: <span class="text-slate-200">{{ (event.data as any).launcherPlayerName || 'Unknown' }}</span>
              | Target: ({{ (event.data as any).coordinates?.x || '?' }}, {{ (event.data as any).coordinates?.y || '?' }})
            </span>
          </template>
          <template v-else-if="event.type === 'ALERT_MIRV' && event.data && typeof event.data === 'object'">
            <span class="font-semibold text-red-400">⚠️ MIRV STRIKE INCOMING!</span>
            <br />
            <span class="text-slate-400">
              From: <span class="text-slate-200">{{ (event.data as any).launcherPlayerName || 'Unknown' }}</span>
              | Target: ({{ (event.data as any).coordinates?.x || '?' }}, {{ (event.data as any).coordinates?.y || '?' }})
            </span>
          </template>
          <template v-else-if="event.type === 'ALERT_LAND' && event.data && typeof event.data === 'object'">
            <span class="font-semibold text-orange-400">⚠️ LAND INVASION DETECTED!</span>
            <br />
            <span class="text-slate-400">
              Attacker: <span class="text-slate-200">{{ (event.data as any).attackerPlayerName || 'Unknown' }}</span>
              | Troops: <span class="text-slate-200">{{ ((event.data as any).troops || 0).toLocaleString() }}</span>
            </span>
          </template>
          <template v-else-if="event.type === 'ALERT_NAVAL' && event.data && typeof event.data === 'object'">
            <span class="font-semibold text-blue-400">⚠️ NAVAL INVASION DETECTED!</span>
            <br />
            <span class="text-slate-400">
              Attacker: <span class="text-slate-200">{{ (event.data as any).attackerPlayerName || 'Unknown' }}</span>
              | Troops: <span class="text-slate-200">{{ ((event.data as any).troops || 0).toLocaleString() }}</span>
            </span>
          </template>
          <template v-else>
            {{ event.message }}
          </template>
        </p>
        </article>
      </div>
      <p v-else class="text-xs text-slate-500">
        No events yet. They will appear here as they arrive.
      </p>
    </div>
  </UCard>
</template>

<script setup lang="ts">
import type { GameEvent, GameEventType } from '~/../../ots-shared/src/game'

const props = defineProps<{
  events: GameEvent[]
}>()

const { events } = toRefs(props)
const scrollContainer = ref<HTMLElement | null>(null)
const autoScroll = ref(true)

// Reverse events so newest appears at bottom
const reversedEvents = computed(() => {
  return [...events.value].reverse()
})

// Auto-scroll to bottom when new events arrive
watch(
  () => events.value.length,
  () => {
    if (autoScroll.value && scrollContainer.value) {
      nextTick(() => {
        if (scrollContainer.value) {
          scrollContainer.value.scrollTop = scrollContainer.value.scrollHeight
        }
      })
    }
  }
)

const eventBadgeClass = (type: GameEventType) => {
  switch (type) {
    case 'ALERT_ATOM':
    case 'ALERT_HYDRO':
    case 'ALERT_MIRV':
      return 'bg-red-500/20 text-red-300'
    case 'ALERT_LAND':
      return 'bg-orange-500/20 text-orange-300'
    case 'ALERT_NAVAL':
      return 'bg-blue-500/20 text-blue-300'
    case 'NUKE_EXPLODED':
    case 'NUKE_INTERCEPTED':
      return 'bg-yellow-500/20 text-yellow-300'
    case 'GAME_START':
    case 'WIN':
      return 'bg-emerald-500/15 text-emerald-300'
    case 'GAME_END':
    case 'LOOSE':
      return 'bg-rose-500/15 text-rose-300'
    case 'HARDWARE_TEST':
      return 'bg-purple-500/20 text-purple-300'
    case 'INFO':
    default:
      return 'bg-slate-700/40 text-slate-200'
  }
}
</script>

<style scoped>
@keyframes slideInHighlight {
  0% {
    opacity: 0;
    transform: translateX(-20px);
    background-color: rgba(34, 211, 238, 0.15);
  }
  50% {
    background-color: rgba(34, 211, 238, 0.15);
  }
  100% {
    opacity: 1;
    transform: translateX(0);
    background-color: transparent;
  }
}

.new-event-animation {
  animation: slideInHighlight 0.6s ease-out;
}
</style>
