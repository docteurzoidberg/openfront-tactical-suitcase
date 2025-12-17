export type TeamScore = {
  teamA: number
  teamB: number
}

export type PlayerInfo = {
  id: string
  name: string
  clanTag?: string
  isAlly: boolean
  score: number
}

export type ModuleGeneralState = {
  m_link: boolean
}

export type ModuleAlertState = {
  m_alert_warning: boolean
  m_alert_atom: boolean
  m_alert_hydro: boolean
  m_alert_mirv: boolean
  m_alert_land: boolean
  m_alert_naval: boolean
}

export type ModuleNukeState = {
  m_nuke_launched: boolean
  m_hydro_launched: boolean
  m_mirv_launched: boolean
}

export type TroopsData = {
  current: number
  max: number
}

export type HWState = {
  m_general: ModuleGeneralState
  m_alert: ModuleAlertState
  m_nuke: ModuleNukeState
}

export type GameState = {
  timestamp: number
  mapName: string
  mode: string
  playerCount: number
  troops?: TroopsData
  hwState: HWState
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

export type SendNukeCommand = {
  action: 'send-nuke'
  params: {
    nukeType: NukeType
  }
}

export type NukeSentEventData = {
  nukeType: NukeType
}

export type IncomingMessage =
  | { type: 'state'; payload: GameState }
  | { type: 'event'; payload: GameEvent }

export type OutgoingMessage =
  | { type: 'cmd'; payload: { action: string; params?: unknown } }
  | { type: 'ack'; payload?: unknown }
