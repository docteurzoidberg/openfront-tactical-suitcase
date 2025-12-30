/**
 * Sidebar Constants - Default values and configuration
 */

import type { LogFilters } from './types'

// ============================================================================
// Log Configuration
// ============================================================================

export const MAX_LOG_ENTRIES = 100

// ============================================================================
// Default Filter Settings
// ============================================================================

export const DEFAULT_LOG_FILTERS: LogFilters = {
  directions: { send: true, recv: true, info: true },
  events: {
    game: true,
    nukes: true,
    alerts: true,
    troops: true,
    sounds: true,
    system: true,
    heartbeat: false
  }
}

export const DEFAULT_SOUND_TOGGLES: Record<string, boolean> = {
  'game_start': true,
  'game_player_death': true,
  'game_victory': true,
  'game_defeat': true
}
