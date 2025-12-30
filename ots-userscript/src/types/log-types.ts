/**
 * Log Types
 */

import type { GameEventType } from '../../../../../ots-shared/src/game'

export type LogDirection = 'send' | 'recv' | 'info'

export type LogFilters = {
  directions: { send: boolean; recv: boolean; info: boolean }
  events: {
    game: boolean
    nukes: boolean
    alerts: boolean
    troops: boolean
    sounds: boolean
    system: boolean
    heartbeat: boolean
  }
}

export type LogEntryMeta = {
  direction: LogDirection
  eventType?: GameEventType
  messageText?: string
}

export type JsonLike = null | undefined | boolean | number | string | JsonLike[] | { [key: string]: JsonLike }
