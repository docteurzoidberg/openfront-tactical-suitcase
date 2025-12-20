/**
 * Safe wrapper around OpenFront.io game API
 * Prevents crashes from missing elements or game API changes
 */

export interface GameAPI {
  isValid(): boolean
  getMyPlayer(): any | null
  getMyPlayerID(): string | null
  getMySmallID(): number | null
  getUnits(...types: string[]): any[]
  getTicks(): number | null
  getX(tile: number): number | null
  getY(tile: number): number | null
  getOwner(tile: number): any | null
  getUnit(unitId: number): any | null
  getPlayerBySmallID(smallID: number): any | null
  // Troop information
  getCurrentTroops(): number | null
  getMaxTroops(): number | null
  getAttackRatio(): number
  getTroopsToSend(): number | null
  // Game state
  isGameStarted(): boolean | null
  // Game updates (polling)
  getUpdatesSinceLastTick(): any | null
  // Game result
  didPlayerWin(): boolean | null
}

/**
 * Get the game instance from the DOM
 */
export function getGameView(): any | null {
  try {
    const eventsDisplay = document.querySelector('events-display')
    if (eventsDisplay && (eventsDisplay as any).game) {
      return (eventsDisplay as any).game
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
  let cachedGame: any | null = null
  let lastCheck = 0
  const CACHE_DURATION = 1000 // Cache for 1 second

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

    getMyPlayer(): any | null {
      try {
        const game = getGame()
        if (!game || typeof game.myPlayer !== 'function') return null
        return game.myPlayer()
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

    getUnits(...types: string[]): any[] {
      try {
        const game = getGame()
        if (!game || typeof game.units !== 'function') return []
        return game.units(...types) || []
      } catch {
        return []
      }
    },

    getTicks(): number | null {
      try {
        const game = getGame()
        if (!game || typeof game.ticks !== 'function') return null
        return game.ticks()
      } catch {
        return null
      }
    },

    getX(tile: number): number | null {
      try {
        const game = getGame()
        if (!game || typeof game.x !== 'function') return null
        return game.x(tile)
      } catch {
        return null
      }
    },

    getY(tile: number): number | null {
      try {
        const game = getGame()
        if (!game || typeof game.y !== 'function') return null
        return game.y(tile)
      } catch {
        return null
      }
    },

    getOwner(tile: number): any | null {
      try {
        const game = getGame()
        if (!game || typeof game.owner !== 'function') return null
        return game.owner(tile)
      } catch {
        return null
      }
    },

    getUnit(unitId: number): any | null {
      try {
        const game = getGame()
        if (!game || typeof game.unit !== 'function') return null
        return game.unit(unitId)
      } catch {
        return null
      }
    },

    getPlayerBySmallID(smallID: number): any | null {
      try {
        const game = getGame()
        if (!game || typeof game.playerBySmallID !== 'function') return null
        return game.playerBySmallID(smallID)
      } catch {
        return null
      }
    },

    getCurrentTroops(): number | null {
      try {
        const myPlayer = this.getMyPlayer()
        if (!myPlayer || typeof myPlayer.troops !== 'function') return null
        return myPlayer.troops()
      } catch {
        return null
      }
    },

    getMaxTroops(): number | null {
      try {
        const game = getGame()
        const myPlayer = this.getMyPlayer()
        if (!game || !myPlayer) return null
        if (typeof game.config !== 'function') return null

        const config = game.config()
        if (!config || typeof config.maxTroops !== 'function') return null

        return config.maxTroops(myPlayer)
      } catch {
        return null
      }
    },

    getAttackRatio(): number {
      // Access UIState from ControlPanel
      const controlPanel = document.querySelector('control-panel') as any
      if (controlPanel && controlPanel.uiState && typeof controlPanel.uiState.attackRatio === 'number') {
        const ratio = controlPanel.uiState.attackRatio
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
        if (typeof game.inSpawnPhase === 'function') {
          return !game.inSpawnPhase()
        }

        // Fallback: check if ticks > 0 (game is running)
        if (typeof game.ticks === 'function') {
          const ticks = game.ticks()
          return ticks !== null && ticks > 0
        }

        return null
      } catch (error) {
        console.error('[GameAPI] Error checking game started:', error)
        return null
      }
    },

    getUpdatesSinceLastTick(): any | null {
      try {
        const game = getGame()
        if (!game || typeof game.updatesSinceLastTick !== 'function') return null
        return game.updatesSinceLastTick()
      } catch (error) {
        console.error('[GameAPI] Error getting updates:', error)
        return null
      }
    },

    didPlayerWin(): boolean | null {
      try {
        const game = (window as any).game
        if (!game) return null

        const myPlayer = game.myPlayer()
        if (!myPlayer) return null

        // Check if game is over
        const gameOver = game.gameOver()
        if (!gameOver) return null

        // If player is eliminated, they lost
        if (myPlayer.eliminated()) {
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

