import type { WsClient } from '../websocket/client'
import type { Hud } from '../hud/main-hud'
import type { GameEvent } from '../../../ots-shared/src/game'
import { waitForElement } from '../utils/dom'
import { createGameAPI, getGameView } from './game-api'
import { NukeTracker } from './nuke-tracker'
import { BoatTracker } from './boat-tracker'
import { LandAttackTracker } from './land-tracker'
import { TroopMonitor } from './troop-monitor'

const CONTROL_PANEL_SELECTOR = 'control-panel'
const POLL_INTERVAL_MS = 100

export class GameBridge {
  private pollInterval: number | null = null
  private nukeTracker: NukeTracker
  private boatTracker: BoatTracker
  private landTracker: LandAttackTracker
  private troopMonitor: TroopMonitor
  private gameConnected = false
  private inGame = false
  private gameAPI = createGameAPI()

  constructor(
    private ws: WsClient,
    private hud: Hud
  ) {
    this.nukeTracker = new NukeTracker()
    this.boatTracker = new BoatTracker()
    this.landTracker = new LandAttackTracker()
    this.troopMonitor = new TroopMonitor(this.gameAPI, this.ws)

    // Register event callbacks
    this.nukeTracker.onEvent((event) => this.handleTrackerEvent(event))
    this.boatTracker.onEvent((event) => this.handleTrackerEvent(event))
    this.landTracker.onEvent((event) => this.handleTrackerEvent(event))
  }

  init() {
    // Wait for the control panel element (indicates game UI is loaded)
    waitForElement(CONTROL_PANEL_SELECTOR, () => {
      console.log('[GameBridge] Control panel detected, waiting for game instance...')
      this.waitForGameAndStart()
    })
  }

  private waitForGameAndStart() {
    // Poll for game instance to be available
    const checkInterval = setInterval(() => {
      const game = getGameView()
      if (game) {
        clearInterval(checkInterval)
        console.log('[GameBridge] Game instance detected, starting trackers')
        this.gameConnected = true
        this.hud.setGameStatus(true)
        this.ws.sendEvent('INFO', 'Game instance detected', { timestamp: Date.now() })
        this.startPolling()
      }
    }, 100)
  }

  private startPolling() {
    if (this.pollInterval) return

    const gameAPI = createGameAPI()

    this.pollInterval = window.setInterval(() => {
      if (!gameAPI.isValid()) {
        // Game became invalid (page navigation, etc.)
        if (this.gameConnected) {
          this.gameConnected = false
          this.hud.setGameStatus(false)
          this.clearTrackers()
        }
        return
      }

      // Update game status if reconnected
      if (!this.gameConnected) {
        this.gameConnected = true
        this.hud.setGameStatus(true)
      }

      const myPlayerID = gameAPI.getMyPlayerID()
      if (!myPlayerID) {
        // Not in a game yet or player not loaded
        if (this.inGame) {
          // Game ended
          this.inGame = false
          this.ws.sendEvent('GAME_END', 'Game ended')
          console.log('[GameBridge] Game ended, clearing trackers')
        }
        this.clearTrackers()
        return
      }

      // Detect game start
      if (!this.inGame) {
        this.inGame = true
        this.ws.sendEvent('GAME_START', 'Game started')
        console.log('[GameBridge] Game started')
        this.clearTrackers() // Clear any stale state

        // Start troop monitoring when game starts
        this.troopMonitor.start()
      }

      try {
        // Run all trackers
        this.nukeTracker.detectLaunches(gameAPI, myPlayerID)
        this.nukeTracker.detectExplosions(gameAPI)

        this.boatTracker.detectLaunches(gameAPI, myPlayerID)
        this.boatTracker.detectArrivals(gameAPI)

        this.landTracker.detectLaunches(gameAPI)
        this.landTracker.detectCompletions(gameAPI)
      } catch (e) {
        console.error('[GameBridge] Error in polling loop:', e)
      }
    }, POLL_INTERVAL_MS)
  }

  private handleTrackerEvent(event: GameEvent) {
    // Forward event to WebSocket
    this.ws.sendEvent(event.type, event.message || '', event.data)
  }

  private clearTrackers() {
    this.nukeTracker.clear()
    this.boatTracker.clear()
    this.landTracker.clear()
    this.troopMonitor.stop()
  }

  handleCommand(action: string, params?: unknown) {
    console.log('[GameBridge] Received command:', action, params)

    if (action === 'send-nuke') {
      this.handleSendNuke(params)
    } else if (action === 'set-attack-ratio') {
      this.handleSetAttackRatio(params)
    } else if (action === 'ping') {
      // Ping is already handled by WsClient, but we can log it
      console.log('[GameBridge] Ping received')
    } else {
      console.warn('[GameBridge] Unknown command:', action)
      this.ws.sendEvent('INFO', `Unknown command: ${action}`, { action, params })
    }
  }

  private handleSetAttackRatio(params?: unknown) {
    const ratio = (params as any)?.ratio

    if (typeof ratio !== 'number' || ratio < 0 || ratio > 1) {
      console.error('[GameBridge] set-attack-ratio command missing or invalid ratio parameter (expected 0-1)')
      this.ws.sendEvent('INFO', 'set-attack-ratio failed: invalid ratio', { params })
      return
    }

    // Convert ratio (0-1) to percentage (1-100)
    const percentage = Math.round(ratio * 100)
    console.log(`[GameBridge] Setting attack ratio to ${ratio} (${percentage}%)`)

    // Update the DOM input element
    const attackRatioInput = document.getElementById('attack-ratio') as HTMLInputElement | null
    if (attackRatioInput) {
      attackRatioInput.value = percentage.toString()

      // Trigger input and change events to notify the game
      attackRatioInput.dispatchEvent(new Event('input', { bubbles: true }))
      attackRatioInput.dispatchEvent(new Event('change', { bubbles: true }))

      console.log('[GameBridge] ✓ Attack ratio slider updated to', percentage, '%')
      this.ws.sendEvent('INFO', 'Attack ratio updated', { ratio, percentage })

      // Force troop monitor to check and send update
      setTimeout(() => {
        this.troopMonitor['checkForChanges'](true)
      }, 100)
    } else {
      console.error('[GameBridge] #attack-ratio input not found')
      this.ws.sendEvent('INFO', 'set-attack-ratio failed: input not found', { ratio })
    }
  }

  private handleSendNuke(params?: unknown) {
    const nukeType = (params as any)?.nukeType

    if (!nukeType) {
      console.error('[GameBridge] send-nuke command missing nukeType parameter')
      this.ws.sendEvent('INFO', 'send-nuke failed: missing nukeType', { params })
      return
    }

    // Try to find the game API for launching nukes
    const game = getGameView()
    if (!game) {
      console.error('[GameBridge] Game not available for nuke launch')
      this.ws.sendEvent('INFO', 'send-nuke failed: game not available', { nukeType })
      return
    }

    // Research needed: Find the actual nuke launch API
    // Common patterns to try:
    // - game.sendNuke?.(type)
    // - game.launchNuke?.(type)
    // - game.useNuke?.(type)
    // - game.player?.sendNuke?.(type)

    console.log('[GameBridge] Attempting to launch nuke:', nukeType)
    console.log('[GameBridge] Game object methods:', Object.keys(game))

    // Try common API patterns
    let success = false
    let method = 'unknown'

    if (typeof game.sendNuke === 'function') {
      try {
        game.sendNuke(nukeType)
        success = true
        method = 'game.sendNuke()'
      } catch (e) {
        console.error('[GameBridge] game.sendNuke() failed:', e)
      }
    } else if (typeof game.launchNuke === 'function') {
      try {
        game.launchNuke(nukeType)
        success = true
        method = 'game.launchNuke()'
      } catch (e) {
        console.error('[GameBridge] game.launchNuke() failed:', e)
      }
    } else if (typeof game.useNuke === 'function') {
      try {
        game.useNuke(nukeType)
        success = true
        method = 'game.useNuke()'
      } catch (e) {
        console.error('[GameBridge] game.useNuke() failed:', e)
      }
    }

    if (success) {
      console.log(`[GameBridge] Nuke launched successfully via ${method}`)

      // Send confirmation event
      let eventType: 'NUKE_LAUNCHED' | 'HYDRO_LAUNCHED' | 'MIRV_LAUNCHED' = 'NUKE_LAUNCHED'
      if (nukeType === 'hydro') {
        eventType = 'HYDRO_LAUNCHED'
      } else if (nukeType === 'mirv') {
        eventType = 'MIRV_LAUNCHED'
      }

      this.ws.sendEvent(eventType, `${nukeType} launched`, { nukeType, method })
    } else {
      console.error('[GameBridge] Nuke launch API not found')
      this.ws.sendEvent('INFO', 'send-nuke failed: API not found', {
        nukeType,
        availableMethods: Object.keys(game).filter(k => typeof (game as any)[k] === 'function').slice(0, 20)
      })
    }
  }

  stop() {
    if (this.pollInterval) {
      clearInterval(this.pollInterval)
      this.pollInterval = null
    }
    this.clearTrackers()
    this.gameConnected = false
    this.hud.setGameStatus(false)
  }
}
