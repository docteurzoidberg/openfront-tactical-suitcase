/**
 * Centralized logging utilities for consistent console output
 */

type LogLevel = 'log' | 'warn' | 'error'

/**
 * Creates a scoped logger with consistent prefix formatting
 */
export function createLogger(scope: string) {
  const prefix = `[${scope}]`

  return {
    log: (...args: unknown[]) => console.log(prefix, ...args),
    warn: (...args: unknown[]) => console.warn(prefix, ...args),
    error: (...args: unknown[]) => console.error(prefix, ...args),

    /**
     * Log with custom level
     */
    logLevel: (level: LogLevel, ...args: unknown[]) => {
      console[level](prefix, ...args)
    },

    /**
     * Log success message with checkmark
     */
    success: (...args: unknown[]) => {
      console.log(`${prefix} ✓`, ...args)
    },

    /**
     * Log failure message with cross
     */
    failure: (...args: unknown[]) => {
      console.log(`${prefix} ✗`, ...args)
    },

    /**
     * Log debug message (only if conditions are met)
     */
    debug: (condition: boolean, ...args: unknown[]) => {
      if (condition) {
        console.log(`${prefix} [DEBUG]`, ...args)
      }
    }
  }
}
