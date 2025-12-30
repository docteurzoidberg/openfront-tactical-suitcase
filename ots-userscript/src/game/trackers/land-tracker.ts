import type { GameAPI } from './game-api'
import type { GameEvent } from '../../../ots-shared/src/game'

interface TrackedAttack {
  attackID: number
  attackerID: number
  attackerPlayerID: string
  attackerPlayerName: string
  troops: number
  targetPlayerID: string
  launchedTick: number
  retreating: boolean
  reported: boolean
}

export class LandAttackTracker {
  private trackedAttacks = new Map<number, TrackedAttack>()
  private eventCallbacks: Array<(event: GameEvent) => void> = []

  onEvent(callback: (event: GameEvent) => void) {
    this.eventCallbacks.push(callback)
  }

  private emitEvent(event: GameEvent) {
    this.eventCallbacks.forEach((cb) => {
      try {
        cb(event)
      } catch (e) {
        console.error('[LandAttackTracker] Error in event callback:', e)
      }
    })
  }

  /**
   * Get player by smallID
   */
  private getPlayerBySmallID(gameAPI: GameAPI, smallID: number): any | null {
    if (!smallID || smallID === 0) return null
    try {
      return gameAPI.getPlayerBySmallID(smallID)
    } catch (e) {
      return null
    }
  }

  /**
   * Detect new land attack launches targeting the current player
   */
  detectLaunches(gameAPI: GameAPI) {
    const myPlayer = gameAPI.getMyPlayer()
    if (!myPlayer) return

    try {
      const incomingAttacks = typeof myPlayer.incomingAttacks === 'function' ? myPlayer.incomingAttacks() : []
      const mySmallID = typeof myPlayer.smallID === 'function' ? myPlayer.smallID() : null

      if (!mySmallID) return

      for (const attack of incomingAttacks) {
        try {
          // Only track attacks targeting the current player (targetID matches my smallID)
          if (attack.targetID === mySmallID && !this.trackedAttacks.has(attack.id)) {
            const attacker = this.getPlayerBySmallID(gameAPI, attack.attackerID)

            if (attacker) {
              const tracked: TrackedAttack = {
                attackID: attack.id,
                attackerID: attack.attackerID,
                attackerPlayerID: typeof attacker.id === 'function' ? attacker.id() : 'unknown',
                attackerPlayerName: typeof attacker.name === 'function' ? attacker.name() : 'Unknown',
                troops: attack.troops || 0,
                targetPlayerID: typeof myPlayer.id === 'function' ? myPlayer.id() : 'unknown',
                launchedTick: gameAPI.getTicks() || 0,
                retreating: attack.retreating || false,
                reported: false
              }

              this.trackedAttacks.set(attack.id, tracked)
              this.reportLaunch(tracked, gameAPI)
            }
          }
        } catch (e) {
          console.error('[LandAttackTracker] Error processing attack:', e)
        }
      }
    } catch (e) {
      console.error('[LandAttackTracker] Error getting incoming attacks:', e)
    }
  }

  /**
   * Detect land attack completions (when attacks disappear from incomingAttacks)
   */
  detectCompletions(gameAPI: GameAPI) {
    const myPlayer = gameAPI.getMyPlayer()
    if (!myPlayer) return

    try {
      const incomingAttacks = typeof myPlayer.incomingAttacks === 'function' ? myPlayer.incomingAttacks() : []
      const activeAttackIDs = new Set(incomingAttacks.map((a: any) => a.id))

      for (const [attackID, tracked] of this.trackedAttacks.entries()) {
        if (!activeAttackIDs.has(attackID)) {
          // Attack is no longer in incomingAttacks - it completed, was cancelled, or retreated
          if (!tracked.reported) {
            // Check if it was retreating when we last saw it
            const wasCancelled = tracked.retreating
            this.reportComplete(tracked, wasCancelled, gameAPI)
            tracked.reported = true
          }
          this.trackedAttacks.delete(attackID)
        } else {
          // Update retreating status
          const currentAttack = incomingAttacks.find((a: any) => a.id === attackID)
          if (currentAttack) {
            tracked.retreating = currentAttack.retreating || false
          }
        }
      }
    } catch (e) {
      console.error('[LandAttackTracker] Error checking attack completions:', e)
    }
  }

  private reportLaunch(tracked: TrackedAttack, gameAPI: GameAPI) {
    const event: GameEvent = {
      type: 'ALERT_LAND',
      timestamp: Date.now(),
      message: 'Land invasion detected!',
      data: {
        type: 'land',
        attackerPlayerID: tracked.attackerPlayerID,
        attackerPlayerName: tracked.attackerPlayerName,
        attackID: tracked.attackID,
        troops: tracked.troops,
        targetPlayerID: tracked.targetPlayerID,
        tick: tracked.launchedTick
      }
    }

    this.emitEvent(event)
  }

  private reportComplete(tracked: TrackedAttack, wasCancelled: boolean, gameAPI: GameAPI) {
    // We don't emit additional events for completions/cancellations
    // The alert LED will stay active for its duration (15s) from the initial ALERT event
    // Just log for debugging
    if (wasCancelled) {
      console.log('[LandAttackTracker] Land attack cancelled:', tracked.attackID)
    } else {
      console.log('[LandAttackTracker] Land attack complete:', tracked.attackID)
    }
  }

  clear() {
    this.trackedAttacks.clear()
  }
}

