import type { GameAPI } from './game-api'
import type { WsClient } from '../websocket/client'

/**
 * Monitors troop data and sends updates only when values change
 * More efficient than polling every 500ms
 */
export class TroopMonitor {
  private pollInterval: number | null = null
  private lastCurrentTroops: number | null = null
  private lastMaxTroops: number | null = null
  private lastAttackRatio: number | null = null
  private lastTroopsToSend: number | null = null

  // Track localStorage for attack ratio changes
  private storageListener: ((e: StorageEvent) => void) | null = null

  constructor(
    private gameAPI: GameAPI,
    private ws: WsClient
  ) { }

  /**
   * Start monitoring for changes
   * Polls at game tick rate (100ms) but only sends when values actually change
   */
  start() {
    if (this.pollInterval) return

    console.log('[TroopMonitor] Starting change detection')

    // Poll at game tick rate (100ms) for efficient change detection
    this.pollInterval = window.setInterval(() => {
      this.checkForChanges()
    }, 100)

    // Listen for localStorage changes (attack ratio slider)
    this.storageListener = (event: StorageEvent) => {
      if (event.key === 'settings.attackRatio' && event.newValue !== event.oldValue) {
        console.log('[TroopMonitor] Attack ratio changed via localStorage')
        // Force a check immediately
        this.checkForChanges(true)
      }
    }
    window.addEventListener('storage', this.storageListener)

    // Watch DOM input for attack ratio changes
    this.interceptAttackRatioChanges()

    // Force initial data read and send
    this.checkForChanges(true)
  }

  /**
   * Stop monitoring
   */
  stop() {
    if (this.pollInterval) {
      clearInterval(this.pollInterval)
      this.pollInterval = null
    }

    if (this.storageListener) {
      window.removeEventListener('storage', this.storageListener)
      this.storageListener = null
    }

    console.log('[TroopMonitor] Stopped')
  }

  /**
   * Check if any values have changed and send update if they have
   */
  private checkForChanges(forceRatioUpdate = false) {
    if (!this.gameAPI.isValid()) {
      // Game not loaded, clear previous values
      if (this.lastCurrentTroops !== null || this.lastMaxTroops !== null) {
        this.lastCurrentTroops = null
        this.lastMaxTroops = null
        this.lastAttackRatio = null
        this.lastTroopsToSend = null
      }
      return
    }

    const currentTroops = this.gameAPI.getCurrentTroops()
    const maxTroops = this.gameAPI.getMaxTroops()
    const attackRatio = this.gameAPI.getAttackRatio()
    const troopsToSend = this.gameAPI.getTroopsToSend()

    // Check if any value changed
    const troopsChanged = currentTroops !== this.lastCurrentTroops
    const maxChanged = maxTroops !== this.lastMaxTroops
    const ratioChanged = attackRatio !== this.lastAttackRatio || forceRatioUpdate
    const toSendChanged = troopsToSend !== this.lastTroopsToSend

    if (troopsChanged || maxChanged || ratioChanged || toSendChanged) {
      // Log what changed
      const changes: string[] = []
      if (troopsChanged) changes.push(`troops: ${this.lastCurrentTroops} → ${currentTroops}`)
      if (maxChanged) changes.push(`max: ${this.lastMaxTroops} → ${maxTroops}`)
      if (ratioChanged) changes.push(`ratio: ${this.lastAttackRatio} → ${attackRatio}`)
      if (toSendChanged) changes.push(`toSend: ${this.lastTroopsToSend} → ${troopsToSend}`)

      console.log('[TroopMonitor] Changes detected:', changes.join(', '))

      // Update stored values
      this.lastCurrentTroops = currentTroops
      this.lastMaxTroops = maxTroops
      this.lastAttackRatio = attackRatio
      this.lastTroopsToSend = troopsToSend

      // Send update
      this.sendUpdate()
    }
  }

  /**
   * Send current troop data to WebSocket
   */
  private sendUpdate() {
    // Divide troops by 10 since game returns 10x the real amount
    const data = {
      currentTroops: this.lastCurrentTroops !== null ? Math.floor(this.lastCurrentTroops / 10) : null,
      maxTroops: this.lastMaxTroops !== null ? Math.floor(this.lastMaxTroops / 10) : null,
      attackRatio: this.lastAttackRatio,
      attackRatioPercent: this.lastAttackRatio !== null ? Math.round(this.lastAttackRatio * 100) : null,
      troopsToSend: this.lastTroopsToSend !== null ? Math.floor(this.lastTroopsToSend / 10) : null,
      timestamp: Date.now()
    }

    this.ws.sendEvent('TROOP_UPDATE', 'Troop data changed', data)
    console.log('[TroopMonitor] Sent update:', data)
  }

  /**
   * Watch DOM input element for attack ratio changes
   */
  private interceptAttackRatioChanges() {
    const self = this

    // Watch for the input element to appear and attach listener
    const attachInputListener = () => {
      const attackRatioInput = document.getElementById('attack-ratio') as HTMLInputElement | null

      if (attackRatioInput) {
        console.log('[TroopMonitor] Found #attack-ratio input, attaching listeners')

        // Listen for input changes (fires while dragging)
        attackRatioInput.addEventListener('input', (event) => {
          const target = event.target as HTMLInputElement
          console.log('[TroopMonitor] ✓ Attack ratio input changed:', target.value, '%')
          self.checkForChanges(true)
        })

        // Listen for change events (fires when drag completes)
        attackRatioInput.addEventListener('change', (event) => {
          const target = event.target as HTMLInputElement
          console.log('[TroopMonitor] ✓ Attack ratio change completed:', target.value, '%')
          self.checkForChanges(true)
        })
      } else {
        // Input not found yet, try again in 500ms
        setTimeout(attachInputListener, 500)
      }
    }

    // Start watching for the input element
    attachInputListener()
  }

  /**
   * Get current troop data snapshot
   */
  getCurrentData() {
    return {
      currentTroops: this.lastCurrentTroops,
      maxTroops: this.lastMaxTroops,
      attackRatio: this.lastAttackRatio,
      attackRatioPercent: this.lastAttackRatio !== null ? Math.round(this.lastAttackRatio * 100) : null,
      troopsToSend: this.lastTroopsToSend
    }
  }
}
