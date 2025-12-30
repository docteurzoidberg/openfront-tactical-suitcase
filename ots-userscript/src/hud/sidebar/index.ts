/**
 * Sidebar HUD Components - Barrel Export
 * 
 * Central export point for all sidebar components, types, and utilities.
 */

// Core components
export { PositionManager } from './position-manager'
export { SettingsPanel } from './settings-panel'
export { FilterPanel } from './filter-panel'
export { HudTemplate } from './hud-template'

// Tab management
export { TabManager, type TabId, type TabConfig, type TabManagerOptions } from './tab-manager'

// Tab components
export { LogsTab } from './tabs/logs-tab'
export {
  HardwareTab,
  tryCaptureHardwareDiagnostic,
  type CapturedHardwareDiagnostic
} from './tabs/hardware-tab'
export { SoundTab } from './tabs/sound-tab'

// Utilities
export { FloatingPanel } from './window'
export { shouldShowLogEntry } from './utils/log-filtering'
export { escapeHtml, formatJson } from './utils/log-format'

// Constants
export { DEFAULT_LOG_FILTERS, DEFAULT_SOUND_TOGGLES } from './constants'

// Types
export type {
  HudPos,
  HudSize,
  LogDirection,
  LogFilters,
  SnapPosition,
  JsonLike
} from './types'
