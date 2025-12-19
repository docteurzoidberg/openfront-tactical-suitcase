import type { GameAPI } from './game-api'
import type { GameEvent } from '../../../ots-shared/src/game'

interface TrackedNuke {
  unitID: number
  type: string
  ownerID: string
  ownerName: string
  targetTile: number
  targetPlayerID: string
  launchedTick: number
  hasReachedTarget: boolean
  reported: boolean
  isOutgoing: boolean // true if player launched it, false if incoming
}

export class NukeTracker {
  private trackedNukes = new Map<number, TrackedNuke>()
  private eventCallbacks: Array<(event: GameEvent) => void> = []

  onEvent(callback: (event: GameEvent) => void) {
    this.eventCallbacks.push(callback)
  }

  private emitEvent(event: GameEvent) {
    this.eventCallbacks.forEach((cb) => {
      try {
        cb(event)
      } catch (e) {
        console.error('[NukeTracker] Error in event callback:', e)
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
   * Detect new nuke launches (both incoming and outgoing)
   */
  detectLaunches(gameAPI: GameAPI, myPlayerID: string) {
    const nukeTypes = ['Atom Bomb', 'Hydrogen Bomb', 'MIRV', 'MIRV Warhead']
    const allNukes = gameAPI.getUnits(...nukeTypes)

    for (const nuke of allNukes) {
      try {
        const nukeId = typeof nuke.id === 'function' ? nuke.id() : null
        if (!nukeId || this.trackedNukes.has(nukeId)) continue

        const owner = typeof nuke.owner === 'function' ? nuke.owner() : null
        const ownerID = owner && typeof owner.id === 'function' ? owner.id() : 'unknown'
        const targetTile = typeof nuke.targetTile === 'function' ? nuke.targetTile() : null
        const targetPlayerID = this.getTargetPlayerID(gameAPI, targetTile)
        const nukeType = typeof nuke.type === 'function' ? nuke.type() : 'Unknown'

        // Track if it's incoming (targeting player) or outgoing (launched by player)
        const isIncoming = targetPlayerID === myPlayerID
        const isOutgoing = ownerID === myPlayerID

        // Track both incoming and outgoing nukes
        if (isIncoming || isOutgoing) {
          const tracked: TrackedNuke = {
            unitID: nukeId,
            type: nukeType,
            ownerID: ownerID,
            ownerName: owner && typeof owner.name === 'function' ? owner.name() : 'Unknown',
            targetTile: targetTile,
            targetPlayerID: targetPlayerID || 'unknown',
            launchedTick: gameAPI.getTicks() || 0,
            hasReachedTarget: false,
            reported: false,
            isOutgoing: isOutgoing
          }

          this.trackedNukes.set(nukeId, tracked)
          this.reportLaunch(tracked, gameAPI)
        }
      } catch (e) {
        console.error('[NukeTracker] Error processing nuke:', e)
      }
    }
  }

  /**
   * Detect nuke explosions or interceptions
   */
  detectExplosions(gameAPI: GameAPI) {
    for (const [unitID, tracked] of this.trackedNukes.entries()) {
      try {
        const unit = gameAPI.getUnit(unitID)

        if (!unit) {
          // Unit was deleted - either intercepted or exploded
          if (!tracked.reported) {
            // Check if it reached target before deletion
            const intercepted = !tracked.hasReachedTarget
            this.reportExplosion(tracked, intercepted, gameAPI)
            tracked.reported = true
          }
          this.trackedNukes.delete(unitID)
          continue
        }

        // Check if nuke reached target
        const reachedTarget = typeof unit.reachedTarget === 'function' ? unit.reachedTarget() : false
        if (reachedTarget && !tracked.hasReachedTarget) {
          // Nuke reached target = explosion!
          tracked.hasReachedTarget = true
          this.reportExplosion(tracked, false, gameAPI)
          tracked.reported = true
          // Unit will be deleted next tick, but we've already reported
        }

        // Update state
        tracked.hasReachedTarget = reachedTarget
      } catch (e) {
        console.error('[NukeTracker] Error checking nuke explosion:', e)
      }
    }
  }

  private reportLaunch(tracked: TrackedNuke, gameAPI: GameAPI) {
    const coordinates = {
      x: gameAPI.getX(tracked.targetTile),
      y: gameAPI.getY(tracked.targetTile)
    }

    if (tracked.isOutgoing) {
      // Player launched this nuke - report as outgoing launch
      let eventType: 'NUKE_LAUNCHED' | 'HYDRO_LAUNCHED' | 'MIRV_LAUNCHED' = 'NUKE_LAUNCHED'

      if (tracked.type.includes('Hydrogen')) {
        eventType = 'HYDRO_LAUNCHED'
      } else if (tracked.type.includes('MIRV')) {
        eventType = 'MIRV_LAUNCHED'
      }

      const event: GameEvent = {
        type: eventType,
        timestamp: Date.now(),
        message: `${tracked.type} launched`,
        data: {
          nukeType: tracked.type,
          nukeUnitID: tracked.unitID,
          targetTile: tracked.targetTile,
          targetPlayerID: tracked.targetPlayerID,
          tick: tracked.launchedTick,
          coordinates
        }
      }
      this.emitEvent(event)
      console.log('[NukeTracker] Player launched:', tracked.type, 'at', coordinates)
    } else {
      // Incoming nuke - report as alert
      let eventType: 'ALERT_ATOM' | 'ALERT_HYDRO' | 'ALERT_MIRV' = 'ALERT_ATOM'
      let message = 'Incoming nuclear strike detected!'

      if (tracked.type.includes('Hydrogen')) {
        eventType = 'ALERT_HYDRO'
        message = 'Incoming hydrogen bomb detected!'
      } else if (tracked.type.includes('MIRV')) {
        eventType = 'ALERT_MIRV'
        message = 'Incoming MIRV strike detected!'
      }

      const event: GameEvent = {
        type: eventType,
        timestamp: Date.now(),
        message: message,
        data: {
          nukeType: tracked.type,
          launcherPlayerID: tracked.ownerID,
          launcherPlayerName: tracked.ownerName,
          nukeUnitID: tracked.unitID,
          targetTile: tracked.targetTile,
          targetPlayerID: tracked.targetPlayerID,
          tick: tracked.launchedTick,
          coordinates
        }
      }
      this.emitEvent(event)
    }
  }

  private reportExplosion(tracked: TrackedNuke, intercepted: boolean, gameAPI: GameAPI) {
    if (tracked.isOutgoing) {
      // Player's nuke - use same event types as incoming for consistency
      const eventType: 'NUKE_EXPLODED' | 'NUKE_INTERCEPTED' = intercepted
        ? 'NUKE_INTERCEPTED'
        : 'NUKE_EXPLODED'

      const event: GameEvent = {
        type: eventType,
        timestamp: Date.now(),
        message: intercepted ? `${tracked.type} intercepted` : `${tracked.type} exploded`,
        data: {
          nukeType: tracked.type,
          unitID: tracked.unitID,
          targetTile: tracked.targetTile,
          targetPlayerID: tracked.targetPlayerID,
          tick: gameAPI.getTicks() || 0,
          isOutgoing: true
        }
      }
      this.emitEvent(event)
      console.log('[NukeTracker] Player nuke', intercepted ? 'intercepted' : 'landed', ':', tracked.type)
    } else {
      // Incoming nuke
      const eventType: 'NUKE_EXPLODED' | 'NUKE_INTERCEPTED' = intercepted
        ? 'NUKE_INTERCEPTED'
        : 'NUKE_EXPLODED'

      const message = intercepted
        ? 'Nuclear weapon intercepted'
        : 'Nuclear weapon exploded'

      const event: GameEvent = {
        type: eventType,
        timestamp: Date.now(),
        message: message,
        data: {
          nukeType: tracked.type,
          unitID: tracked.unitID,
          ownerID: tracked.ownerID,
          ownerName: tracked.ownerName,
          targetTile: tracked.targetTile,
          tick: gameAPI.getTicks() || 0
        }
      }

      this.emitEvent(event)
      console.log('[NukeTracker] Incoming nuke', intercepted ? 'intercepted' : 'exploded', ':', tracked.type)
    }
  }

  clear() {
    this.trackedNukes.clear()
  }
}

