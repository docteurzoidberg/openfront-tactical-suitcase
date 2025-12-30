import type { LogDirection, LogFilters } from '../types'
import type { GameEventType } from '../../../../../ots-shared/src/game'
import { shouldShowLogEntry } from '../utils/log-filtering'
import { escapeHtml, formatJson } from '../utils/log-format'
import { MAX_LOG_ENTRIES } from '../constants'

/**
 * Logs tab component - displays WebSocket communication logs
 */
export class LogsTab {
  private logList: HTMLDivElement
  private autoScrollBtn: HTMLButtonElement
  private filterBtn: HTMLButtonElement
  private logCounter = 0
  private autoScroll = true
  private logFilters: LogFilters

  constructor(
    private onFilterClick: () => void,
    initialFilters: LogFilters
  ) {
    this.logFilters = initialFilters

    // Get DOM elements
    this.logList = document.getElementById('ots-hud-log') as HTMLDivElement
    this.autoScrollBtn = document.getElementById('ots-hud-autoscroll') as HTMLButtonElement
    this.filterBtn = document.getElementById('ots-hud-filter') as HTMLButtonElement

    if (!this.logList || !this.autoScrollBtn || !this.filterBtn) {
      throw new Error('Logs tab DOM elements not found')
    }

    // Setup event listeners
    this.attachListeners()
    this.updateAutoScrollBtn()
  }

  /**
   * Create HTML for logs tab
   */
  static createHTML(): string {
    return `
      <div id="ots-tab-logs" class="ots-tab-content" style="flex:1;display:flex;flex-direction:column;min-height:0;">
        <div id="ots-hud-log" style="flex:1;overflow-y:auto;overflow-x:hidden;padding:8px;font-size:11px;line-height:1.4;background:rgba(10,10,15,0.8);"></div>
        <div id="ots-hud-footer" style="flex:none;padding:6px 8px;display:flex;align-items:center;justify-content:space-between;background:rgba(10,10,15,0.8);border-top:1px solid rgba(255,255,255,0.1);">
          <button id="ots-hud-filter" style="all:unset;cursor:pointer;font-size:11px;color:#e0e0e0;padding:3px 8px;border-radius:4px;background:rgba(255,255,255,0.1);transition:background 0.2s;">⚑ Filter</button>
          <button id="ots-hud-autoscroll" style="all:unset;cursor:pointer;font-size:10px;font-weight:600;color:#6ee7b7;padding:3px 8px;border-radius:12px;background:rgba(52,211,153,0.2);white-space:nowrap;transition:background 0.2s;">✓ Auto</button>
        </div>
      </div>
    `
  }

  /**
   * Attach event listeners
   */
  private attachListeners() {
    // Auto-scroll button
    this.autoScrollBtn.addEventListener('click', () => {
      this.autoScroll = !this.autoScroll
      this.updateAutoScrollBtn()
    })

    // Filter button hover effects
    this.filterBtn.addEventListener('mouseenter', () => {
      this.filterBtn.style.background = 'rgba(255, 255, 255, 0.2)'
    })
    this.filterBtn.addEventListener('mouseleave', () => {
      this.filterBtn.style.background = 'rgba(255, 255, 255, 0.1)'
    })
    this.filterBtn.addEventListener('click', () => {
      this.onFilterClick()
    })
  }

  /**
   * Update auto-scroll button appearance
   */
  private updateAutoScrollBtn() {
    if (this.autoScroll) {
      this.autoScrollBtn.textContent = '✓ Auto'
      this.autoScrollBtn.style.color = '#6ee7b7'
      this.autoScrollBtn.style.background = 'rgba(52,211,153,0.2)'
    } else {
      this.autoScrollBtn.textContent = '✗ Auto'
      this.autoScrollBtn.style.color = '#9ca3af'
      this.autoScrollBtn.style.background = 'rgba(100,116,139,0.2)'
    }
  }

  /**
   * Update filters (from external source)
   */
  updateFilters(filters: LogFilters) {
    this.logFilters = filters
  }

  /**
   * Add a log entry
   */
  pushLog(direction: LogDirection, text: string, eventType?: GameEventType, jsonData?: unknown) {
    if (!this.logList) return

    // Check filters
    if (!shouldShowLogEntry(this.logFilters, direction, eventType, text)) {
      return
    }

    this.logCounter++
    const entry = document.createElement('div')
    entry.dataset.logId = `${this.logCounter}`
    entry.dataset.direction = direction
    if (eventType) entry.dataset.eventType = eventType
    entry.style.cssText = 'margin-bottom:6px;'

    let directionColor = '#93c5fd' // send (blue)
    let directionLabel = 'SEND'
    if (direction === 'recv') {
      directionColor = '#86efac' // recv (green)
      directionLabel = 'RECV'
    } else if (direction === 'info') {
      directionColor = '#fcd34d' // info (yellow)
      directionLabel = 'INFO'
    }

    const lines = []

    // Direction tag
    lines.push(`<span style="display:inline-block;font-weight:600;font-size:9px;padding:1px 4px;border-radius:3px;background:${directionColor};color:#0f172a;margin-right:6px;">${directionLabel}</span>`)

    // Event type badge (if present)
    if (eventType) {
      let eventColor = '#60a5fa' // default blue
      if (eventType.startsWith('ALERT_')) eventColor = '#f87171' // red for alerts
      else if (eventType.includes('NUKE')) eventColor = '#fb923c' // orange for nukes
      else if (eventType.includes('TROOPS')) eventColor = '#4ade80' // green for troops
      else if (eventType === 'SOUND_PLAY') eventColor = '#fbbf24' // yellow for sounds
      else if (eventType.includes('HARDWARE')) eventColor = '#06b6d4' // cyan for hardware

      lines.push(`<span style="display:inline-block;font-size:9px;padding:1px 4px;border-radius:3px;background:${eventColor};color:white;margin-right:6px;">${eventType}</span>`)
    }

    // Message text
    lines.push(`<span style="color:#d1d5db;">${escapeHtml(text)}</span>`)

    // JSON data (collapsible)
    if (jsonData !== undefined) {
      const jsonId = `ots-log-json-${this.logCounter}`
      lines.push(`<div id="${jsonId}" style="margin-top:4px;padding:6px;background:rgba(0,0,0,0.3);border-radius:4px;cursor:pointer;overflow:hidden;max-height:24px;transition:max-height 0.2s;" data-collapsed="true">`)
      lines.push(`<div style="font-size:10px;color:#9ca3af;margin-bottom:4px;">▶ JSON</div>`)
      lines.push(`<pre style="margin:0;font-size:10px;line-height:1.4;color:#e5e7eb;white-space:pre-wrap;word-break:break-all;">${formatJson(jsonData as any)}</pre>`)
      lines.push(`</div>`)

      // Add click handler after rendering
      setTimeout(() => {
        const jsonEl = document.getElementById(jsonId)
        if (jsonEl) {
          jsonEl.addEventListener('click', () => {
            const collapsed = jsonEl.dataset.collapsed === 'true'
            if (collapsed) {
              jsonEl.style.maxHeight = 'none'
              jsonEl.dataset.collapsed = 'false'
              const arrow = jsonEl.querySelector('div')
              if (arrow) arrow.textContent = '▼ JSON'
            } else {
              jsonEl.style.maxHeight = '24px'
              jsonEl.dataset.collapsed = 'true'
              const arrow = jsonEl.querySelector('div')
              if (arrow) arrow.textContent = '▶ JSON'
            }
          })
        }
      }, 0)
    }

    entry.innerHTML = lines.join('')
    this.logList.appendChild(entry)

    // Trim to configured maximum
    while (this.logList.children.length > MAX_LOG_ENTRIES) {
      this.logList.removeChild(this.logList.firstChild!)
    }

    // Auto-scroll
    if (this.autoScroll) {
      this.logList.scrollTop = this.logList.scrollHeight
    }
  }

  /**
   * Clear all logs
   */
  clearLogs() {
    if (this.logList) {
      this.logList.innerHTML = ''
      this.logCounter = 0
    }
  }

  /**
   * Refilter existing logs
   */
  refilterLogs() {
    if (!this.logList) return

    Array.from(this.logList.children).forEach(child => {
      const entry = child as HTMLElement
      const direction = entry.dataset.direction as LogDirection | undefined
      const eventType = entry.dataset.eventType as GameEventType | undefined
      const messageText = entry.textContent || ''

      if (direction && shouldShowLogEntry(this.logFilters, direction, eventType, messageText)) {
        entry.style.display = ''
      } else {
        entry.style.display = 'none'
      }
    })
  }
}
