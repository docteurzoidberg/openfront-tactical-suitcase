/**
 * Game module constants
 * 
 * Magic numbers and strings used across game integration components
 */

// ============================================================================
// DOM Selectors
// ============================================================================

export const CONTROL_PANEL_SELECTOR = 'control-panel'
export const ATTACK_RATIO_INPUT_SELECTOR = '#attack-ratio'

// ============================================================================
// Polling & Timing
// ============================================================================

export const GAME_POLL_INTERVAL_MS = 100
export const GAME_INSTANCE_CHECK_INTERVAL_MS = 100
export const INPUT_LISTENER_RETRY_DELAY_MS = 500

// ============================================================================
// Game API Cache
// ============================================================================

export const API_CACHE_DURATION_MS = 1000 // Cache for 1 second

// ============================================================================
// Game Update Types (from OpenFront.io internal enum)
// ============================================================================

export const GAME_UPDATE_TYPE = {
  WIN: 13, // GameUpdateType.Win = 13
  // Add other types as needed
} as const

// ============================================================================
// WebSocket Close Codes
// ============================================================================

export const WS_CLOSE_CODE_URL_CHANGED = 4100

// ============================================================================
// UI Limits
// ============================================================================

export const MAX_LOG_ENTRIES = 100

// ============================================================================
// Reconnection Settings
// ============================================================================

export const DEFAULT_RECONNECT_DELAY_MS = 2000
export const MAX_RECONNECT_DELAY_MS = 15000
