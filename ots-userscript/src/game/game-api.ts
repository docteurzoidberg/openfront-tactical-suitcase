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
    }
  }
}

