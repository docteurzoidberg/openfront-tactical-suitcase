/**
 * Safe wrapper around OpenFront.io game API
 * Prevents crashes from missing elements or game API changes
 */

import { API_CACHE_DURATION_MS } from './constants'

type AnyFn = (...args: unknown[]) => unknown

function isFn(value: unknown): value is AnyFn {
  return typeof value === 'function'
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null
}

// ============================================================================
// Minimal OpenFront Facade Types (only what we use)
// ============================================================================

export type GameUpdates = { [key: number]: unknown } & { [key: string]: unknown }

export interface OwnerLike {
  isPlayer?: () => boolean
  id?: () => string
  name?: () => string
}

export interface IncomingAttackLike {
  id: number
  attackerID: number
  targetID: number
  troops?: number
  retreating?: boolean
}

export interface BuildableUnitLike {
  type: string
  canBuild: unknown
}

export interface PlayerActionsLike {
  buildableUnits: BuildableUnitLike[]
}

export interface PlayerLike extends OwnerLike {
  smallID?: () => number
  team?: () => number | null
  clientID?: () => string
  troops?: () => number
  eliminated?: () => boolean
  isAlive?: () => boolean
  hasSpawned?: () => boolean
  incomingAttacks?: () => IncomingAttackLike[]
  actions?: (arg: unknown) => Promise<PlayerActionsLike>
}

export interface UnitLike {
  id?: () => number
  type?: () => string
  owner?: () => PlayerLike | null
  targetTile?: () => number
  troops?: () => number
  reachedTarget?: () => boolean
}

export interface GameConfigLike {
  maxTroops?: (player: PlayerLike) => number
}

export interface GameViewLike {
  myPlayer?: () => PlayerLike | null
  units?: (...types: string[]) => UnitLike[]
  unit?: (unitId: number) => UnitLike | null
  owner?: (tile: number) => OwnerLike | null
  playerBySmallID?: (smallID: number) => PlayerLike | null
  ticks?: () => number
  x?: (tile: number) => number
  y?: (tile: number) => number
  config?: () => GameConfigLike
  updatesSinceLastTick?: () => GameUpdates
  inSpawnPhase?: () => boolean
  gameOver?: () => boolean
}

export interface GameAPI {
  isValid(): boolean
  getMyPlayer(): PlayerLike | null
  getMyPlayerID(): string | null
  getMySmallID(): number | null
  getUnits(...types: string[]): UnitLike[]
  getTicks(): number | null
  getX(tile: number): number | null
  getY(tile: number): number | null
  getOwner(tile: number): OwnerLike | null
  getUnit(unitId: number): UnitLike | null
  getPlayerBySmallID(smallID: number): PlayerLike | null
  // Troop information
  getCurrentTroops(): number | null
  getMaxTroops(): number | null
  getAttackRatio(): number
  getTroopsToSend(): number | null
  // Game state
  isGameStarted(): boolean | null
  hasSpawned(): boolean | null
  // Game updates (polling)
  getUpdatesSinceLastTick(): GameUpdates | null
  // Game result
  didPlayerWin(): boolean | null
}

/**
 * Get the game instance from the DOM
 */
export function getGameView(): GameViewLike | null {
  try {
    const eventsDisplay = document.querySelector('events-display')
    if (eventsDisplay && isRecord(eventsDisplay) && 'game' in eventsDisplay) {
      const maybeGame = (eventsDisplay as unknown as { game?: unknown }).game
      if (maybeGame && isRecord(maybeGame)) {
        return maybeGame as GameViewLike
      }
    }
  } catch (e) {
    // Silently handle errors
  }
  return null
}

/**
 * Create a safe wrapper around the game instance
 */
export function createGameAPI(): GameAPI {
  let cachedGame: GameViewLike | null = null
  let lastCheck = 0
  const CACHE_DURATION = API_CACHE_DURATION_MS

  const getGame = () => {
    const now = Date.now()
    if (!cachedGame || now - lastCheck > CACHE_DURATION) {
      cachedGame = getGameView()
      lastCheck = now
    }
    return cachedGame
  }

  return {
    isValid(): boolean {
      const game = getGame()
      return game != null
    },

    getMyPlayer(): PlayerLike | null {
      try {
        const game = getGame()
        if (!game || !isFn(game.myPlayer)) return null
        const myPlayer = game.myPlayer() as unknown
        return isRecord(myPlayer) ? (myPlayer as PlayerLike) : null
      } catch {
        return null
      }
    },

    getMyPlayerID(): string | null {
      try {
        const myPlayer = this.getMyPlayer()
        if (!myPlayer || typeof myPlayer.id !== 'function') return null
        return myPlayer.id()
      } catch {
        return null
      }
    },

    getMySmallID(): number | null {
      try {
        const myPlayer = this.getMyPlayer()
        if (!myPlayer || typeof myPlayer.smallID !== 'function') return null
        return myPlayer.smallID()
      } catch {
        return null
      }
    },

    getUnits(...types: string[]): UnitLike[] {
      try {
        const game = getGame()
        if (!game || !isFn(game.units)) return []
        const units = game.units(...types) as unknown
        return Array.isArray(units) ? (units as UnitLike[]) : []
      } catch {
        return []
      }
    },

    getTicks(): number | null {
      try {
        const game = getGame()
        if (!game || !isFn(game.ticks)) return null
        const ticks = game.ticks() as unknown
        return typeof ticks === 'number' ? ticks : null
      } catch {
        return null
      }
    },

    getX(tile: number): number | null {
      try {
        const game = getGame()
        if (!game || !isFn(game.x)) return null
        const x = game.x(tile) as unknown
        return typeof x === 'number' ? x : null
      } catch {
        return null
      }
    },

    getY(tile: number): number | null {
      try {
        const game = getGame()
        if (!game || !isFn(game.y)) return null
        const y = game.y(tile) as unknown
        return typeof y === 'number' ? y : null
      } catch {
        return null
      }
    },

    getOwner(tile: number): OwnerLike | null {
      try {
        const game = getGame()
        if (!game || !isFn(game.owner)) return null
        const owner = game.owner(tile) as unknown
        return isRecord(owner) ? (owner as OwnerLike) : null
      } catch {
        return null
      }
    },

    getUnit(unitId: number): UnitLike | null {
      try {
        const game = getGame()
        if (!game || !isFn(game.unit)) return null
        const unit = game.unit(unitId) as unknown
        return isRecord(unit) ? (unit as UnitLike) : null
      } catch {
        return null
      }
    },

    getPlayerBySmallID(smallID: number): PlayerLike | null {
      try {
        const game = getGame()
        if (!game || !isFn(game.playerBySmallID)) return null
        const player = game.playerBySmallID(smallID) as unknown
        return isRecord(player) ? (player as PlayerLike) : null
      } catch {
        return null
      }
    },

    getCurrentTroops(): number | null {
      try {
        const myPlayer = this.getMyPlayer()
        if (!myPlayer || !isFn(myPlayer.troops)) return null
        const troops = myPlayer.troops() as unknown
        return typeof troops === 'number' ? troops : null
      } catch {
        return null
      }
    },

    getMaxTroops(): number | null {
      try {
        const game = getGame()
        const myPlayer = this.getMyPlayer()
        if (!game || !myPlayer) return null
        if (!isFn(game.config)) return null

        const config = game.config() as unknown
        if (!isRecord(config) || !isFn((config as GameConfigLike).maxTroops)) return null

        const maxTroops = (config as GameConfigLike).maxTroops!(myPlayer)
        return typeof maxTroops === 'number' ? maxTroops : null
      } catch {
        return null
      }
    },

    getAttackRatio(): number {
      // Access UIState from ControlPanel
      const controlPanel = document.querySelector('control-panel')
      const uiState = isRecord(controlPanel) ? (controlPanel as unknown as { uiState?: unknown }).uiState : undefined
      const ratio = isRecord(uiState) ? uiState.attackRatio : undefined
      if (typeof ratio === 'number') {
        if (ratio >= 0 && ratio <= 1) {
          return ratio
        }
      }

      throw new Error('Unable to access attackRatio from UIState')
    },

    getTroopsToSend(): number | null {
      try {
        const currentTroops = this.getCurrentTroops()
        if (currentTroops === null) return null

        const attackRatio = this.getAttackRatio()
        return Math.floor(currentTroops * attackRatio)
      } catch {
        return null
      }
    },

    isGameStarted(): boolean | null {
      try {
        const game = getGame()
        if (!game) return null

        // Use inSpawnPhase() to determine if spawn countdown is active
        // Returns false during spawn phase, true after spawn phase ends
        if (isFn(game.inSpawnPhase)) {
          const inSpawnPhase = game.inSpawnPhase() as unknown
          return typeof inSpawnPhase === 'boolean' ? !inSpawnPhase : null
        }

        // Fallback: check if ticks > 0 (game is running)
        if (isFn(game.ticks)) {
          const ticks = game.ticks() as unknown
          return typeof ticks === 'number' ? ticks > 0 : null
        }

        return null
      } catch (error) {
        console.error('[GameAPI] Error checking game started:', error)
        return null
      }
    },

    hasSpawned(): boolean | null {
      try {
        const myPlayer = this.getMyPlayer()
        if (!myPlayer) return null

        // Check if player has spawned (selected a spawn tile)
        if (isFn(myPlayer.hasSpawned)) {
          const spawned = myPlayer.hasSpawned() as unknown
          return typeof spawned === 'boolean' ? spawned : null
        }

        return null
      } catch (error) {
        console.error('[GameAPI] Error checking hasSpawned:', error)
        return null
      }
    },

    getUpdatesSinceLastTick(): GameUpdates | null {
      try {
        const game = getGame()
        if (!game || !isFn(game.updatesSinceLastTick)) return null
        const updates = game.updatesSinceLastTick() as unknown
        return isRecord(updates) ? (updates as GameUpdates) : null
      } catch (error) {
        console.error('[GameAPI] Error getting updates:', error)
        return null
      }
    },

    didPlayerWin(): boolean | null {
      try {
        const game = getGame()
        if (!game || !isFn(game.myPlayer)) return null

        const myPlayerUnknown = game.myPlayer() as unknown
        if (!isRecord(myPlayerUnknown)) return null
        const myPlayer = myPlayerUnknown as PlayerLike

        // Check if game is over
        if (!isFn(game.gameOver)) return null
        const gameOver = game.gameOver() as unknown
        if (gameOver !== true) return null

        // If player is eliminated, they lost
        if (isFn(myPlayer.eliminated) && myPlayer.eliminated() === true) {
          return false
        }

        // If game is over and player is not eliminated, they won
        return true
      } catch (error) {
        console.error('[GameAPI] Error checking win status:', error)
        return null
      }
    }
  }
}

