// ============================================================================
// Enums & Union Types
// ============================================================================

export type GamePhase = 'lobby' | 'spawning' | 'in-game' | 'game-won' | 'game-lost'

export type NukeType = 'atom' | 'hydro' | 'mirv'

export type WsStatus = 'DISCONNECTED' | 'CONNECTING' | 'OPEN' | 'ERROR'

export type GameEventType =
  | 'INFO'
  | 'ERROR'
  | 'GAME_SPAWNING'
  | 'GAME_START'
  | 'GAME_END'
  | 'SOUND_PLAY'
  | 'HARDWARE_DIAGNOSTIC'
  | 'NUKE_LAUNCHED'
  | 'NUKE_EXPLODED'
  | 'NUKE_INTERCEPTED'
  | 'ALERT_NUKE'
  | 'ALERT_HYDRO'
  | 'ALERT_MIRV'
  | 'ALERT_LAND'
  | 'ALERT_NAVAL'
  | 'TROOP_UPDATE'
  | 'HARDWARE_TEST'

// ============================================================================
// Game State Types
// ============================================================================

export type TroopsData = {
  current: number
  max: number
}

export type GameState = {
  timestamp: number
  mapName: string
  mode: string
  playerCount: number
  troops?: TroopsData
}

// ============================================================================
// Event Types
// ============================================================================

export type GameEvent = {
  type: GameEventType
  timestamp: number
  message?: string
  data?: unknown
}

export type SoundPriority = 'low' | 'normal' | 'high'

export type SoundPlayEventData = {
  soundId: string
  priority?: SoundPriority
  interrupt?: boolean
  requestId?: string
  context?: Record<string, unknown>
}

export type NukeSentEventData = {
  nukeType: NukeType
}

export type HardwareComponentStatus = {
  present: boolean
  working: boolean
}

export type HardwareDiagnosticData = {
  version: string
  deviceType: 'firmware' | 'simulator'
  serialNumber: string
  owner: string
  hardware: {
    lcd: HardwareComponentStatus
    inputBoard: HardwareComponentStatus
    outputBoard: HardwareComponentStatus
    adc: HardwareComponentStatus
    soundModule: HardwareComponentStatus
  }
}

// ============================================================================
// Protocol Message Types
// ============================================================================

export type IncomingMessage =
  | { type: 'state'; payload: GameState }
  | { type: 'event'; payload: GameEvent }

export type OutgoingMessage =
  | { type: 'cmd'; payload: { action: string; params?: unknown } }
  | { type: 'ack'; payload?: unknown }

// ============================================================================
// Protocol Constants
// ============================================================================

export const PROTOCOL_CONSTANTS = {
  // WebSocket configuration
  DEFAULT_WS_URL: 'ws://localhost:3000/ws',
  DEFAULT_WS_PORT: 3000,

  // Client types for handshake
  CLIENT_TYPE_UI: 'ui' as const,
  CLIENT_TYPE_USERSCRIPT: 'userscript' as const,
  CLIENT_TYPE_FIRMWARE: 'firmware' as const,

  // Standard INFO event messages
  INFO_MESSAGE_USERSCRIPT_CONNECTED: 'userscript-connected',
  INFO_MESSAGE_USERSCRIPT_DISCONNECTED: 'userscript-disconnected',
  INFO_MESSAGE_NUKE_SENT: 'Nuke sent',

  // Heartbeat configuration
  HEARTBEAT_INTERVAL_MS: 5000,
  RECONNECT_DELAY_MS: 2000,
  RECONNECT_MAX_DELAY_MS: 15000,
} as const
