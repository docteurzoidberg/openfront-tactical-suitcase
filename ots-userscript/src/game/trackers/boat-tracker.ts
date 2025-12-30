import type { GameAPI } from './game-api'
import type { GameEvent } from '../../../ots-shared/src/game'

interface TrackedBoat {
  unitID: number
  ownerID: string
  ownerName: string
  troops: number
  targetTile: number
  targetPlayerID: string
  launchedTick: number
  hasReachedTarget: boolean
  reported: boolean
}

export class BoatTracker {
  private trackedBoats = new Map<number, TrackedBoat>()
  private eventCallbacks: Array<(event: GameEvent) => void> = []

  onEvent(callback: (event: GameEvent) => void) {
    this.eventCallbacks.push(callback)
  }

  private emitEvent(event: GameEvent) {
    this.eventCallbacks.forEach((cb) => {
      try {
        cb(event)
      } catch (e) {
        console.error('[BoatTracker] Error in event callback:', e)
      }
    })
  }

  /**
   * Get target player ID from a tile
   */
  private getTargetPlayerID(gameAPI: GameAPI, targetTile: number): string | null {
    if (!targetTile) return null
    try {
      const owner = gameAPI.getOwner(targetTile)
      if (owner && typeof owner.isPlayer === 'function' && owner.isPlayer()) {
        return owner.id()
      }
    } catch (e) {
      // Target might be terra nullius or invalid
    }
    return null
  }

  /**
   * Detect new boat launches targeting the current player
   */
  detectLaunches(gameAPI: GameAPI, myPlayerID: string) {
    const transportShips = gameAPI.getUnits('Transport')

    for (const boat of transportShips) {
      try {
        const boatId = typeof boat.id === 'function' ? boat.id() : null
        if (!boatId || this.trackedBoats.has(boatId)) continue

        const targetTile = typeof boat.targetTile === 'function' ? boat.targetTile() : null
        const targetPlayerID = this.getTargetPlayerID(gameAPI, targetTile)

        // Only track boats targeting the current player
        if (targetPlayerID === myPlayerID) {
          const owner = typeof boat.owner === 'function' ? boat.owner() : null
          const troops = typeof boat.troops === 'function' ? boat.troops() : 0

          const tracked: TrackedBoat = {
            unitID: boatId,
            ownerID: owner && typeof owner.id === 'function' ? owner.id() : 'unknown',
            ownerName: owner && typeof owner.name === 'function' ? owner.name() : 'Unknown',
            troops: troops,
            targetTile: targetTile,
            targetPlayerID: targetPlayerID,
            launchedTick: gameAPI.getTicks() || 0,
            hasReachedTarget: false,
            reported: false
          }

          this.trackedBoats.set(boatId, tracked)
          this.reportLaunch(tracked, gameAPI)
        }
      } catch (e) {
        console.error('[BoatTracker] Error processing boat:', e)
      }
    }
  }

  /**
   * Detect boat arrivals or destructions
   */
  detectArrivals(gameAPI: GameAPI) {
    for (const [unitID, tracked] of this.trackedBoats.entries()) {
      try {
        const unit = gameAPI.getUnit(unitID)

        if (!unit) {
          // Unit was deleted
          if (!tracked.reported) {
            // Unit was deleted before reaching target = destroyed
            this.reportArrival(tracked, true, gameAPI)
            tracked.reported = true
          }
          this.trackedBoats.delete(unitID)
          continue
        }

        // Check if boat reached target
        const reachedTarget = typeof unit.reachedTarget === 'function' ? unit.reachedTarget() : false
        if (reachedTarget && !tracked.hasReachedTarget) {
          // Boat reached target = arrival!
          tracked.hasReachedTarget = true
          this.reportArrival(tracked, false, gameAPI)
          tracked.reported = true
          // Unit will be deleted next tick, but we've already reported
        }

        // Update state
        tracked.hasReachedTarget = reachedTarget
      } catch (e) {
        console.error('[BoatTracker] Error checking boat arrival:', e)
      }
    }
  }

  private reportLaunch(tracked: TrackedBoat, gameAPI: GameAPI) {
    const coordinates = {
      x: gameAPI.getX(tracked.targetTile),
      y: gameAPI.getY(tracked.targetTile)
    }

    const event: GameEvent = {
      type: 'ALERT_NAVAL',
      timestamp: Date.now(),
      message: 'Naval invasion detected!',
      data: {
        type: 'boat',
        attackerPlayerID: tracked.ownerID,
        attackerPlayerName: tracked.ownerName,
        transportShipUnitID: tracked.unitID,
        troops: tracked.troops,
        targetTile: tracked.targetTile,
        targetPlayerID: tracked.targetPlayerID,
        tick: tracked.launchedTick,
        coordinates
      }
    }

    this.emitEvent(event)
  }

  private reportArrival(tracked: TrackedBoat, destroyed: boolean, gameAPI: GameAPI) {
    // We don't emit additional events for arrivals/destructions
    // The alert LED will stay active for its duration (15s) from the initial ALERT event
    // Just log for debugging
    if (destroyed) {
      console.log('[BoatTracker] Boat destroyed:', tracked.unitID)
    } else {
      console.log('[BoatTracker] Boat arrived:', tracked.unitID)
    }
  }

  clear() {
    this.trackedBoats.clear()
  }
}

