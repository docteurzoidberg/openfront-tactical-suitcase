<template>
  <UCard>
    <template #header>
      <div class="flex flex-col gap-3">
        <div class="flex items-center justify-between">
          <h2 class="text-sm font-semibold text-slate-300">Recent Events</h2>
          <div class="flex items-center gap-2">
            <UButton
              icon="i-heroicons-funnel"
              size="xs"
              color="gray"
              variant="ghost"
              @click="showFilters = !showFilters"
              :ui="{ rounded: 'rounded-md' }"
            >
              {{ showFilters ? 'Hide Filters' : 'Filters' }}
            </UButton>
            <UButton
              icon="i-heroicons-arrow-path"
              size="xs"
              color="gray"
              variant="ghost"
              @click="clearAllEvents"
              :ui="{ rounded: 'rounded-md' }"
            >
              Clear
            </UButton>
            <button
              @click="scrollToBottom"
              :class="[
                'rounded px-2 py-1 text-xs font-medium transition-colors',
                autoScroll
                  ? 'bg-emerald-500/20 text-emerald-300 hover:bg-emerald-500/30'
                  : 'bg-amber-500/20 text-amber-300 hover:bg-amber-500/30'
              ]"
              :title="autoScroll ? 'Auto-scrolling enabled' : 'Paused - Click to scroll to bottom'"
            >
              {{ autoScroll ? '✓ Auto-scroll' : '⏸ Paused' }}
            </button>
          </div>
        </div>

        <!-- Filter Panel -->
        <div v-if="showFilters" class="space-y-3 rounded-lg bg-slate-900/50 p-3">
          <div class="flex items-center justify-between">
            <span class="text-xs font-medium text-slate-400">Event Types</span>
            <div class="flex gap-2">
              <UButton
                size="2xs"
                color="gray"
                variant="ghost"
                @click="selectAllFilters"
              >
                All
              </UButton>
              <UButton
                size="2xs"
                color="gray"
                variant="ghost"
                @click="deselectAllFilters"
              >
                None
              </UButton>
            </div>
          </div>
          
          <div class="grid grid-cols-2 gap-2">
            <UCheckbox
              v-for="(filter, index) in eventFilters"
              :key="index"
              v-model="filter.enabled"
            >
              <template #label>
                <span class="flex items-center gap-1.5 text-xs text-slate-300">
                  <span
                    class="inline-block h-2 w-2 rounded-full"
                    :class="filter.color"
                  />
                  <span>{{ filter.label }}</span>
                  <span class="text-slate-500">({{ getEventCount(filter.types) }})</span>
                </span>
              </template>
            </UCheckbox>
          </div>
        </div>
      </div>
    </template>

    <div 
      ref="scrollContainer" 
      class="h-[calc(100vh-16rem)] overflow-y-auto pr-2"
      @scroll="handleScroll"
    >
      <div v-if="filteredEvents.length" class="space-y-2 text-xs">
        <article
          v-for="(event, index) in filteredEvents"
          :key="`${event.timestamp}-${event.type}-${JSON.stringify(event.data || {})}`"
          :class="[
            'cursor-pointer rounded border border-dashed border-slate-800/60 bg-slate-950/40 px-3 py-2 transition-all hover:border-slate-700/80 hover:bg-slate-900/50',
            index === filteredEvents.length - 1 && 'new-event-animation'
          ]"
          @click.stop="toggleExpanded(`${event.timestamp}-${event.type}`)"
        >
          <header class="mb-1 flex items-center justify-between gap-2">
            <div class="flex items-center gap-2">
              <span class="text-slate-500">
                {{ expandedEvents.has(`${event.timestamp}-${event.type}`) ? '▼' : '▶' }}
              </span>
              <span
                class="inline-flex items-center rounded px-1.5 py-0.5 text-[10px] font-semibold uppercase tracking-wide"
                :class="eventBadgeClass(event.type)"
              >
                {{ event.type }}
              </span>
            </div>
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
          <template v-else-if="event.type === 'CMD_SENT' && event.data && typeof event.data === 'object'">
            <span class="font-semibold text-blue-400">📤 {{ event.message }}</span>
            <br />
            <span class="text-slate-400 text-xs">
              Action: <span class="font-mono text-slate-200">{{ (event.data as any).action }}</span>
            </span>
          </template>
          <template v-else-if="event.type === 'ALERT_NUKE' && event.data && typeof event.data === 'object'">
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
        
        <!-- Expanded Data View -->
        <div 
          v-if="expandedEvents.has(`${event.timestamp}-${event.type}`) && event.data"
          class="mt-2 rounded border border-slate-800/40 bg-slate-950/60 p-2"
        >
          <pre class="text-[10px] text-slate-400 overflow-x-auto">{{ JSON.stringify(event.data, null, 2) }}</pre>
        </div>
        </article>
      </div>
      <p v-else-if="events.length === 0" class="text-xs text-slate-500">
        No events yet. They will appear here as they arrive.
      </p>
      <p v-else class="text-xs text-slate-500">
        No events match the current filters. {{ events.length }} event(s) hidden.
      </p>
    </div>
  </UCard>
</template>

<script setup lang="ts">
import type { GameEvent, GameEventType } from '~/../../ots-shared/src/game'

const props = defineProps<{
  events: GameEvent[]
}>()

const emit = defineEmits<{
  'clear-events': []
}>()

const { events } = toRefs(props)
const scrollContainer = ref<HTMLElement | null>(null)
const autoScroll = ref(true)
const showFilters = ref(false)
const expandedEvents = ref<Set<string>>(new Set())
const isUserScrolling = ref(false)
const scrollTimeout = ref<NodeJS.Timeout | null>(null)

// Define event filter interface and data
interface EventFilter {
  types: (GameEventType | 'OTHER')[]
  label: string
  enabled: boolean
  color: string
}

const eventFilters = ref<EventFilter[]>([
  { types: ['ALERT_NUKE', 'ALERT_HYDRO', 'ALERT_MIRV', 'ALERT_LAND', 'ALERT_NAVAL'], label: 'Incoming Threats', enabled: true, color: 'bg-red-500' },
  { types: ['NUKE_LAUNCHED', 'NUKE_EXPLODED', 'NUKE_INTERCEPTED'], label: 'Nukes Sent', enabled: true, color: 'bg-orange-500' },
  { types: ['CMD_SENT'], label: 'Commands', enabled: true, color: 'bg-blue-500' },
  { types: ['TROOP_UPDATE'], label: 'Troop Updates', enabled: true, color: 'bg-purple-500' },
  { types: ['GAME_START', 'GAME_END', 'WIN', 'LOOSE'], label: 'Game Events', enabled: true, color: 'bg-emerald-500' },
  { types: ['INFO'], label: 'Info', enabled: true, color: 'bg-slate-500' },
  { types: ['ERROR'], label: 'Errors', enabled: true, color: 'bg-red-400' },
  { types: ['OTHER'], label: 'Other', enabled: true, color: 'bg-slate-600' },
])

// Get count of events by type group
const getEventCount = (types: (GameEventType | 'OTHER')[]) => {
  if (types.includes('OTHER')) {
    const knownTypes = new Set(
      eventFilters.value.flatMap(f => f.types).filter(t => t !== 'OTHER')
    )
    return events.value.filter(e => !knownTypes.has(e.type)).length
  }
  return events.value.filter(e => types.includes(e.type)).length
}

// Filter events based on enabled filters
const filteredEvents = computed(() => {
  const enabledTypes = new Set(
    eventFilters.value
      .filter(f => f.enabled)
      .flatMap(f => f.types)
  )
  
  const knownTypes = new Set(
    eventFilters.value
      .flatMap(f => f.types)
      .filter(t => t !== 'OTHER')
  )
  
  const reversed = [...events.value].reverse()
  
  return reversed.filter(event => {
    // Check if it's a known type
    if (knownTypes.has(event.type)) {
      return enabledTypes.has(event.type)
    }
    // Unknown type - include if "OTHER" is enabled
    return enabledTypes.has('OTHER')
  })
})

// Select/deselect all filters
const selectAllFilters = () => {
  eventFilters.value.forEach(f => f.enabled = true)
}

const deselectAllFilters = () => {
  eventFilters.value.forEach(f => f.enabled = false)
}

// Clear all events
const clearAllEvents = () => {
  emit('clear-events')
}

// Toggle event expansion
const toggleExpanded = (eventId: string) => {
  if (expandedEvents.value.has(eventId)) {
    expandedEvents.value.delete(eventId)
  } else {
    expandedEvents.value.add(eventId)
  }
}

// Manually scroll to bottom and re-enable auto-scroll
const scrollToBottom = () => {
  autoScroll.value = true
  isUserScrolling.value = false
  if (scrollContainer.value) {
    nextTick(() => {
      if (scrollContainer.value) {
        scrollContainer.value.scrollTop = scrollContainer.value.scrollHeight
      }
    })
  }
}

// Handle scroll to detect user scrolling
const handleScroll = () => {
  if (!scrollContainer.value) return
  
  const container = scrollContainer.value
  const isAtBottom = container.scrollHeight - container.scrollTop - container.clientHeight < 50
  
  // If user is at the bottom, enable auto-scroll
  if (isAtBottom) {
    autoScroll.value = true
    isUserScrolling.value = false
  } else {
    // User scrolled up, disable auto-scroll
    if (autoScroll.value) {
      autoScroll.value = false
      isUserScrolling.value = true
    }
  }
  
  // Clear any existing timeout
  if (scrollTimeout.value) {
    clearTimeout(scrollTimeout.value)
  }
}

// Save/load filter preferences from localStorage
const FILTER_STORAGE_KEY = 'ots-event-filters-v2'

onMounted(() => {
  try {
    const saved = localStorage.getItem(FILTER_STORAGE_KEY)
    if (saved) {
      const savedFilters = JSON.parse(saved) as Record<string, boolean>
      eventFilters.value.forEach((filter, index) => {
        const key = `group-${index}`
        if (key in savedFilters) {
          filter.enabled = savedFilters[key]
        }
      })
    }
  } catch (e) {
    console.error('Failed to load event filters:', e)
  }
})

// Save filter state to localStorage when changed
watch(
  () => eventFilters.value.map(f => f.enabled),
  () => {
    try {
      const filterState = eventFilters.value.reduce((acc, filter, index) => {
        acc[`group-${index}`] = filter.enabled
        return acc
      }, {} as Record<string, boolean>)
      localStorage.setItem(FILTER_STORAGE_KEY, JSON.stringify(filterState))
    } catch (e) {
      console.error('Failed to save event filters:', e)
    }
  },
  { deep: true }
)

// Auto-scroll to bottom when new events arrive or filters change
watch(
  () => [events.value.length, eventFilters.value.map(f => f.enabled)],
  () => {
    if (autoScroll.value && scrollContainer.value) {
      nextTick(() => {
        if (scrollContainer.value) {
          scrollContainer.value.scrollTop = scrollContainer.value.scrollHeight
        }
      })
    }
  },
  { deep: true }
)

const eventBadgeClass = (type: GameEventType) => {
  switch (type) {
    case 'ALERT_NUKE':
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
    case 'ERROR':
      return 'bg-red-400/20 text-red-300 font-semibold'
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
