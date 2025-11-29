import { computed, ref, watchEffect } from 'vue'
import { useWebSocket } from '@vueuse/core'
import type { GameEvent, GameState, IncomingMessage, OutgoingMessage } from '../../ots-shared/src/game'

const WS_URL =
  typeof window !== 'undefined'
    ? (window.location.protocol === 'https:' ? 'wss://' : 'ws://') + window.location.host + '/ws-ui'
    : ''

const latestState = ref<GameState | null>(null)
const events = ref<GameEvent[]>([])
const lastUserscriptHeartbeat = ref<number | null>(null)
const lastUserscriptHeartbeatId = ref<number>(0)
const userscriptWsOpen = ref(false)

export function useGameSocket() {
  const { status, data, send, open, close } = useWebSocket(WS_URL, {
    autoReconnect: {
      retries: 10,
      delay: 2000,
      onFailed() {
        // noop for now
      }
    }
  })

  watchEffect(() => {
    const raw = data.value
    if (!raw) return

    try {
      const msg = JSON.parse(raw as string) as IncomingMessage
      if (msg.type === 'state') {
        latestState.value = msg.payload
      } else if (msg.type === 'event') {
        events.value = [msg.payload, ...events.value].slice(0, 100)

        if (msg.payload.type === 'INFO') {
          const message = msg.payload.message

          if (message === 'userscript-ws-open') {
            userscriptWsOpen.value = true
          } else if (message === 'userscript-ws-close') {
            userscriptWsOpen.value = false
          }

          lastUserscriptHeartbeat.value = Date.now()
          lastUserscriptHeartbeatId.value++
        }
      }
    } catch {
      // ignore malformed messages
    }
  })

  const connectionStatus = computed(() => status.value)

  const userscriptStatus = computed(() => {
    if (!lastUserscriptHeartbeat.value) return 'UNKNOWN'
    if (!userscriptWsOpen.value) return 'OFFLINE'

    const diff = Date.now() - lastUserscriptHeartbeat.value
    if (diff < 10_000) return 'ONLINE'
    if (diff < 60_000) return 'STALE'
    return 'OFFLINE'
  })

  const userscriptHeartbeatId = computed(() => lastUserscriptHeartbeatId.value)

  const sendMessage = (msg: OutgoingMessage) => {
    send(JSON.stringify(msg))
  }

  return {
    status: connectionStatus,
    userscriptStatus,
    userscriptHeartbeatId,
    latestState,
    events,
    send: sendMessage,
    open,
    close
  }
}
