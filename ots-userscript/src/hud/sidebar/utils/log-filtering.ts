import type { GameEventType } from '../../../../ots-shared/src/game'
import type { LogDirection, LogFilters } from './types'

export function shouldShowLogEntry(
  filters: LogFilters,
  direction: LogDirection,
  eventType?: GameEventType,
  messageText?: string
): boolean {
  if (!filters.directions[direction]) return false

  // If no event type, show (it's a raw message)
  if (!eventType) return true

  // Heartbeat specifically (INFO event with message containing 'heartbeat')
  if (eventType === 'INFO' && messageText && messageText.includes('"heartbeat"')) {
    return filters.events.heartbeat
  }

  if (eventType.startsWith('GAME_') || eventType === 'GAME_SPAWNING') {
    return filters.events.game
  } else if (eventType.includes('NUKE') || eventType.includes('HYDRO') || eventType.includes('MIRV')) {
    return filters.events.nukes
  } else if (eventType.startsWith('ALERT_')) {
    return filters.events.alerts
  } else if (eventType.includes('TROOP')) {
    return filters.events.troops
  } else if (eventType === 'SOUND_PLAY') {
    return filters.events.sounds
  } else if (eventType === 'INFO' || eventType === 'ERROR' || eventType === 'HARDWARE_TEST') {
    return filters.events.system
  }

  return true
}
