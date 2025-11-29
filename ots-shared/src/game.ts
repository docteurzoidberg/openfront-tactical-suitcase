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

export type GameState = {
  timestamp: number
  mapName: string
  mode: string
  players: PlayerInfo[]
  score: TeamScore
}

export type GameEventType = 'KILL' | 'DEATH' | 'OBJECTIVE' | 'INFO' | 'ERROR'

export type GameEvent = {
  type: GameEventType
  timestamp: number
  message: string
  data?: unknown
}

export type IncomingMessage =
  | { type: 'state'; payload: GameState }
  | { type: 'event'; payload: GameEvent }

export type OutgoingMessage =
  | { type: 'cmd'; payload: { action: string; params?: unknown } }
  | { type: 'ack'; payload?: unknown }
