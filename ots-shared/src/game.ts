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

export type GameEventType =
  | 'INFO'
  | 'GAME_START'
  | 'GAME_END'
  | 'WIN'
  | 'LOOSE'
  | 'NUKE_LAUNCHED'
  | 'HYDRO_LAUNCHED'
  | 'MIRV_LAUNCHED'
  | 'NUKE_EXPLODED'
  | 'NUKE_INTERCEPTED'
  | 'ALERT_ATOM'
  | 'ALERT_HYDRO'
  | 'ALERT_MIRV'
  | 'ALERT_LAND'
  | 'ALERT_NAVAL'
  | 'HARDWARE_TEST'

export type GameEvent = {
  type: GameEventType
  timestamp: number
  message?: string
  data?: unknown
}

// Game phase tracking
export type GamePhase = 'lobby' | 'spawning' | 'in-game' | 'game-won' | 'game-lost'

// Nuke types for hardware module
export type NukeType = 'atom' | 'hydro' | 'mirv'

// WebSocket connection status
export type WsStatus = 'DISCONNECTED' | 'CONNECTING' | 'OPEN' | 'ERROR'

export type NukeSentEventData = {
  nukeType: NukeType
}

export type IncomingMessage =
  | { type: 'state'; payload: GameState }
  | { type: 'event'; payload: GameEvent }

export type OutgoingMessage =
  | { type: 'cmd'; payload: { action: string; params?: unknown } }
  | { type: 'ack'; payload?: unknown }

// Protocol Constants
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
