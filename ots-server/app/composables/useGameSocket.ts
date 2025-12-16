import { computed, ref, watchEffect } from 'vue'
import { useWebSocket } from '@vueuse/core'
import type { GameEvent, IncomingMessage, OutgoingMessage, NukeType, NukeSentEventData, TroopsData } from '../../../ots-shared/src/game'

const WS_URL =
  typeof window !== 'undefined'
    ? (window.location.protocol === 'https:' ? 'wss://' : 'ws://') + window.location.host + '/ws'
    : ''

const events = ref<GameEvent[]>([])
const lastUserscriptHeartbeat = ref<number | null>(null)
const lastUserscriptHeartbeatId = ref<number>(0)

// Nuke blink state
const activeNukes = ref<{ atom: boolean; hydro: boolean; mirv: boolean }>({
  atom: false,
  hydro: false,
  mirv: false,
})

const blinkTimers = new Map<NukeType, NodeJS.Timeout>()

// Power state
const powerOn = ref<boolean>(true)

// Alert state
const activeAlerts = ref<{
  warning: boolean
  atom: boolean
  hydro: boolean
  mirv: boolean
  land: boolean
  naval: boolean
}>({
  warning: false,
  atom: false,
  hydro: false,
  mirv: false,
  land: false,
  naval: false
})

const alertTimers = new Map<string, NodeJS.Timeout>()

// Troops state
const troops = ref<TroopsData | null>(null)

// Game state
const inGame = ref<boolean>(false)

export function useGameSocket() {
  const { status, data, send, open, close } = useWebSocket(WS_URL, {
    autoReconnect: {
      retries: 10,
      delay: 2000,
      onFailed() {
        // noop for now
      }
    },
    onConnected() {
      // Send handshake to identify as UI client
      send(JSON.stringify({ type: 'handshake', clientType: 'ui' }))
    }
  })

  watchEffect(() => {
    const raw = data.value
    if (!raw) return

    try {
      const msg = JSON.parse(raw as string) as IncomingMessage
      if (msg.type === 'state') {
        // Handle game state updates
        if (msg.payload.troops) {
          troops.value = msg.payload.troops
        }
      } else if (msg.type === 'event') {
        events.value = [msg.payload, ...events.value].slice(0, 100)

        if (msg.payload.type === 'INFO') {
          const message = msg.payload.message

          if (message === 'Nuke sent') {
            // Handle nuke-sent event
            const data = msg.payload.data as NukeSentEventData | undefined
            if (data?.nukeType) {
              startNukeBlink(data.nukeType)
            }
          }

          // Handle userscript disconnect
          if (message === 'userscript-disconnected') {
            lastUserscriptHeartbeat.value = null
            lastUserscriptHeartbeatId.value++
          }
          // Handle userscript connect or other INFO messages as heartbeat
          else {
            lastUserscriptHeartbeat.value = Date.now()
            lastUserscriptHeartbeatId.value++
          }
        }

        // Handle game state events
        if (msg.payload.type === 'GAME_START') {
          inGame.value = true
          // Clear all alerts and nukes on game start
          Object.keys(activeAlerts.value).forEach(key => {
            activeAlerts.value[key as keyof typeof activeAlerts.value] = false
          })
          Object.keys(activeNukes.value).forEach(key => {
            activeNukes.value[key as keyof typeof activeNukes.value] = false
          })
          // Clear all timers
          blinkTimers.forEach(timer => clearTimeout(timer))
          blinkTimers.clear()
          alertTimers.forEach(timer => clearTimeout(timer))
          alertTimers.clear()
        } else if (msg.payload.type === 'GAME_END') {
          inGame.value = false
        }

        // Handle alert events
        if (msg.payload.type === 'ALERT_ATOM') {
          startAlert('atom')
        } else if (msg.payload.type === 'ALERT_HYDRO') {
          startAlert('hydro')
        } else if (msg.payload.type === 'ALERT_MIRV') {
          startAlert('mirv')
        } else if (msg.payload.type === 'ALERT_LAND') {
          startAlert('land')
        } else if (msg.payload.type === 'ALERT_NAVAL') {
          startAlert('naval')
        }
      }
    } catch {
      // ignore malformed messages
    }
  })

  const startNukeBlink = (nukeType: NukeType) => {
    // Clear existing timer if any
    const existingTimer = blinkTimers.get(nukeType)
    if (existingTimer) {
      clearTimeout(existingTimer)
    }

    // Start blinking
    activeNukes.value[nukeType as keyof typeof activeNukes.value] = true

    // Stop blinking after 4 seconds
    const timer = setTimeout(() => {
      activeNukes.value[nukeType as keyof typeof activeNukes.value] = false
      blinkTimers.delete(nukeType)
    }, 4000)

    blinkTimers.set(nukeType, timer)
  }

  const startAlert = (alertType: 'atom' | 'hydro' | 'mirv' | 'land' | 'naval') => {
    // Clear existing timer if any
    const existingTimer = alertTimers.get(alertType)
    if (existingTimer) {
      clearTimeout(existingTimer)
    }

    // Activate alert
    activeAlerts.value[alertType] = true
    updateWarningLED()

    // Deactivate after duration (10 seconds for nukes, 15 for invasions)
    const duration = (alertType === 'land' || alertType === 'naval') ? 15000 : 10000
    const timer = setTimeout(() => {
      activeAlerts.value[alertType] = false
      alertTimers.delete(alertType)
      updateWarningLED()
    }, duration)

    alertTimers.set(alertType, timer)
  }

  const updateWarningLED = () => {
    // WARNING LED is active if ANY other alert is active
    const anyAlertActive = activeAlerts.value.atom ||
      activeAlerts.value.hydro ||
      activeAlerts.value.mirv ||
      activeAlerts.value.land ||
      activeAlerts.value.naval
    activeAlerts.value.warning = anyAlertActive
  }

  const uiStatus = computed(() => status.value)

  const userscriptStatus = computed(() => {
    if (!lastUserscriptHeartbeat.value) return 'UNKNOWN'

    const diff = Date.now() - lastUserscriptHeartbeat.value
    if (diff < 10_000) return 'ONLINE'
    if (diff < 60_000) return 'STALE'
    return 'OFFLINE'
  })

  const gameStatus = computed(() => {
    return inGame.value ? 'IN_GAME' : 'WAITING'
  })

  const userscriptHeartbeatId = computed(() => lastUserscriptHeartbeatId.value)

  const sendMessage = (msg: OutgoingMessage) => {
    send(JSON.stringify(msg))
  }

  const sendNukeCommand = (nukeType: NukeType) => {
    sendMessage({
      type: 'cmd',
      payload: {
        action: 'send-nuke',
        params: { nukeType }
      }
    })
  }

  const togglePower = () => {
    powerOn.value = !powerOn.value
  }

  const sendSetTroopsPercent = (percent: number) => {
    sendMessage({
      type: 'cmd',
      payload: {
        action: 'set-troops-percent',
        params: { percent }
      }
    })
  }

  return {
    uiStatus,
    userscriptStatus,
    gameStatus,
    userscriptHeartbeatId,
    events,
    activeNukes,
    activeAlerts,
    powerOn,
    troops,
    send: sendMessage,
    sendNukeCommand,
    sendSetTroopsPercent,
    togglePower,
    open,
    close
  }
}
