import { computed, ref, watchEffect } from 'vue'
import { useWebSocket } from '@vueuse/core'
import type { GameEvent, IncomingMessage, OutgoingMessage, NukeType, NukeSentEventData, TroopsData, GamePhase } from '../../../ots-shared/src/game'
import { PROTOCOL_CONSTANTS } from '../../../ots-shared/src/game'

const WS_URL =
  typeof window !== 'undefined'
    ? (window.location.protocol === 'https:' ? 'wss://' : 'ws://') + window.location.host + '/ws'
    : ''

const events = ref<GameEvent[]>([])
const lastUserscriptHeartbeat = ref<number | null>(null)
const lastUserscriptHeartbeatId = ref<number>(0)

// Nuke tracking state - track each individual nuke by ID
interface TrackedNuke {
  id: string
  type: 'atom' | 'hydro' | 'mirv'
  launchedAt: number
}

const trackedNukes = ref<Map<string, TrackedNuke>>(new Map())

const activeNukes = computed(() => ({
  atom: Array.from(trackedNukes.value.values()).some(n => n.type === 'atom'),
  hydro: Array.from(trackedNukes.value.values()).some(n => n.type === 'hydro'),
  mirv: Array.from(trackedNukes.value.values()).some(n => n.type === 'mirv'),
}))

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
const attackRatio = ref<number | null>(null)  // Attack ratio percentage (0-100)

// Game phase tracking - null means userscript not connected
const gamePhase = ref<GamePhase | null>(null)

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
      send(JSON.stringify({ type: 'handshake', clientType: PROTOCOL_CONSTANTS.CLIENT_TYPE_UI }))
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
        events.value = [msg.payload, ...events.value]

        if (msg.payload.type === 'INFO') {
          const message = msg.payload.message

          // Handle userscript disconnect
          if (message === PROTOCOL_CONSTANTS.INFO_MESSAGE_USERSCRIPT_DISCONNECTED) {
            lastUserscriptHeartbeat.value = null
            lastUserscriptHeartbeatId.value++
            gamePhase.value = null // Reset to unknown when userscript disconnects
          }
          // Handle userscript connect - initialize to lobby
          else if (message === PROTOCOL_CONSTANTS.INFO_MESSAGE_USERSCRIPT_CONNECTED) {
            lastUserscriptHeartbeat.value = Date.now()
            lastUserscriptHeartbeatId.value++
            gamePhase.value = 'lobby' // Default to lobby when userscript connects
          }
          // Handle other INFO messages as heartbeat
          else {
            lastUserscriptHeartbeat.value = Date.now()
            lastUserscriptHeartbeatId.value++
          }
        }

        // Handle game state events
        if (msg.payload.type === 'GAME_START') {
          gamePhase.value = 'in-game'
          // Clear all alerts and nukes on game start
          Object.keys(activeAlerts.value).forEach(key => {
            activeAlerts.value[key as keyof typeof activeAlerts.value] = false
          })
          trackedNukes.value.clear()
          // Clear all timers
          alertTimers.forEach(timer => clearTimeout(timer))
          alertTimers.clear()
        } else if (msg.payload.type === 'GAME_END') {
          gamePhase.value = 'lobby'
        } else if (msg.payload.type === 'WIN') {
          gamePhase.value = 'game-won'
        } else if (msg.payload.type === 'LOOSE') {
          gamePhase.value = 'game-lost'
        }

        // Handle troop update events
        if (msg.payload.type === 'TROOP_UPDATE') {
          const data = msg.payload.data as any
          if (data && typeof data.currentTroops === 'number' && typeof data.maxTroops === 'number') {
            troops.value = {
              current: data.currentTroops,
              max: data.maxTroops
            }
            // Update attack ratio if provided (convert from 0-1 to 0-100)
            if (typeof data.attackRatioPercent === 'number') {
              attackRatio.value = data.attackRatioPercent
            }
          }
        }

        // Handle nuke launch events (outgoing nukes launched by player)
        if (msg.payload.type === 'NUKE_LAUNCHED' || msg.payload.type === 'HYDRO_LAUNCHED' || msg.payload.type === 'MIRV_LAUNCHED') {
          const data = msg.payload.data as any
          if (data?.nukeUnitID) {
            let nukeType: 'atom' | 'hydro' | 'mirv' = 'atom'
            if (msg.payload.type === 'HYDRO_LAUNCHED') nukeType = 'hydro'
            else if (msg.payload.type === 'MIRV_LAUNCHED') nukeType = 'mirv'

            trackNukeLaunch(data.nukeUnitID, nukeType)
          }
        }

        // Handle nuke completion events (exploded or intercepted)
        if (msg.payload.type === 'NUKE_EXPLODED' || msg.payload.type === 'NUKE_INTERCEPTED') {
          const data = msg.payload.data as any
          if (data?.unitID && data?.isOutgoing) {
            // Only stop tracking if it's our outgoing nuke
            stopTrackingNuke(data.unitID)
          }
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

  const trackNukeLaunch = (nukeUnitID: string, nukeType: 'atom' | 'hydro' | 'mirv') => {
    console.log(`[Nuke Tracking] Tracking ${nukeType} nuke: ${nukeUnitID}`)
    trackedNukes.value.set(nukeUnitID, {
      id: nukeUnitID,
      type: nukeType,
      launchedAt: Date.now()
    })
  }

  const stopTrackingNuke = (nukeUnitID: string) => {
    const nuke = trackedNukes.value.get(nukeUnitID)
    if (nuke) {
      console.log(`[Nuke Tracking] Stopped tracking ${nuke.type} nuke: ${nukeUnitID}`)
      trackedNukes.value.delete(nukeUnitID)
    }
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
    // Return UNKNOWN if userscript not connected
    if (userscriptStatus.value === 'UNKNOWN' || userscriptStatus.value === 'OFFLINE' || gamePhase.value === null) {
      return 'UNKNOWN'
    }

    // Map phase to display status
    switch (gamePhase.value) {
      case 'lobby':
        return 'LOBBY'
      case 'spawning':
        return 'SPAWNING'
      case 'in-game':
        return 'IN_GAME'
      case 'game-won':
        return 'WON'
      case 'game-lost':
        return 'LOST'
      default:
        return 'UNKNOWN'
    }
  })

  const isInGame = computed(() => gamePhase.value === 'in-game')

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
    // Convert percentage (0-100) to ratio (0-1) for userscript
    const ratio = percent / 100
    sendMessage({
      type: 'cmd',
      payload: {
        action: 'set-attack-ratio',
        params: { ratio }
      }
    })
    console.log('[useGameSocket] Sending set-attack-ratio command:', { percent, ratio })
  }
  const clearEvents = () => {
    events.value = []
    console.log('[useGameSocket] Cleared all events')
  }
  return {
    uiStatus,
    userscriptStatus,
    gameStatus,
    gamePhase,
    isInGame,
    userscriptHeartbeatId,
    events,
    activeNukes,
    activeAlerts,
    powerOn,
    troops,
    attackRatio,
    send: sendMessage,
    sendNukeCommand,
    sendSetTroopsPercent,
    togglePower,
    clearEvents,
    open,
    close
  }
}
