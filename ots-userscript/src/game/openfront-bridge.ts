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

// GameUpdateType enum from OpenFront.io (src/core/game/GameUpdates.ts)
const GameUpdateType = {
  Win: 12  // GameUpdateType.Win enum value
} as const

export class GameBridge {
  private pollInterval: number | null = null
  private nukeTracker: NukeTracker
  private boatTracker: BoatTracker
  private landTracker: LandAttackTracker
  private troopMonitor: TroopMonitor
  private gameConnected = false
  private inGame = false
  private inSpawning = false
  private gameAPI = createGameAPI()
  private hasProcessedWin = false

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

  private pollForWinUpdates(gameAPI: ReturnType<typeof createGameAPI>) {
    try {
      // Skip if already processed
      if (this.hasProcessedWin) return

      const myPlayer = gameAPI.getMyPlayer()
      if (!myPlayer) return

      // Check for player death (immediate detection)
      const isAlive = myPlayer.isAlive ? myPlayer.isAlive() : true
      const hasSpawned = myPlayer.hasSpawned ? myPlayer.hasSpawned() : false
      const game = getGameView()
      const inSpawnPhase = game && game.inSpawnPhase ? game.inSpawnPhase() : false

      if (!isAlive && !inSpawnPhase && hasSpawned && this.inGame) {
        // Player died during the game
        this.hasProcessedWin = true
        this.ws.sendEvent('GAME_END', 'You died', { victory: false, phase: 'game-lost', reason: 'death' })
        console.log('[GameBridge] ✗ You died!')
        this.inGame = false
        this.inSpawning = false
        return
      }

      // Get updates since last tick
      const updates = gameAPI.getUpdatesSinceLastTick()
      if (!updates) return

      // Check for Win updates (GameUpdateType.Win = 12)
      const winUpdates = updates[GameUpdateType.Win]
      if (!winUpdates || !Array.isArray(winUpdates) || winUpdates.length === 0) return

      // Process win update (only once)
      this.hasProcessedWin = true

      const winUpdate = winUpdates[0]

      // Determine if we won or lost
      let didWin = false
      const winner = winUpdate.winner

      if (winner) {
        const myClientID = myPlayer.clientID ? myPlayer.clientID() : null
        const myTeam = myPlayer.team ? myPlayer.team() : null

        if (winner[0] === 'player') {
          // Individual win - check if we're the winner
          didWin = winner[1] === myClientID
        } else if (winner[0] === 'team') {
          // Team win - check if we're on the winning team
          didWin = winner[1] === myTeam
        }
      }

      // Double-check death status
      if (!isAlive) {
        didWin = false
      }

      // Send game end event
      if (didWin) {
        this.ws.sendEvent('GAME_END', 'Victory!', { victory: true, phase: 'game-won', winner: winner })
        console.log('[GameBridge] ✓ Game ended - VICTORY!')
      } else {
        this.ws.sendEvent('GAME_END', 'Defeat', { victory: false, phase: 'game-lost', winner: winner })
        console.log('[GameBridge] ✗ Game ended - DEFEAT')
      }

      this.inGame = false
      this.inSpawning = false
    } catch (error) {
      console.error('[GameBridge] Error polling win updates:', error)
    }
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

      // Poll for win updates (Option 3: Most reliable method)
      this.pollForWinUpdates(gameAPI)

      const myPlayerID = gameAPI.getMyPlayerID()
      if (!myPlayerID) {
        // Not in a game yet or player not loaded
        if (this.inGame || this.inSpawning) {
          // Game ended - reset state
          this.inGame = false
          this.inSpawning = false
          this.hasProcessedWin = false
          console.log('[GameBridge] Player no longer in game')
        }
        this.clearTrackers()
        return
      }

      // Check if game has truly started (spawn countdown ended)
      const gameStarted = gameAPI.isGameStarted()

      // Detect spawning phase (player exists but game hasn't started)
      if (!this.inSpawning && !this.inGame && gameStarted === false) {
        this.inSpawning = true
        this.ws.sendEvent('GAME_SPAWNING', 'Spawn countdown active', { spawning: true })
        console.log('[GameBridge] Spawning phase - countdown active')
        this.clearTrackers() // Clear any stale state
      }

      // Detect game truly starting (spawn countdown ended)
      // Only trigger if we haven't already processed a game end
      if (gameStarted === true && !this.inGame && !this.hasProcessedWin) {
        this.inGame = true
        this.inSpawning = false
        this.ws.sendEvent('GAME_START', 'Game started - countdown ended')
        console.log('[GameBridge] Game started - spawn countdown ended')
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
      this.ws.sendEvent('ERROR', `Unknown command: ${action}`, { action, params })
    }
  }

  private handleSetAttackRatio(params?: unknown) {
    const ratio = (params as any)?.ratio

    if (typeof ratio !== 'number' || ratio < 0 || ratio > 1) {
      console.error('[GameBridge] set-attack-ratio command missing or invalid ratio parameter (expected 0-1)')
      this.ws.sendEvent('ERROR', 'set-attack-ratio failed: invalid ratio', { params })
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
      this.ws.sendEvent('ERROR', 'set-attack-ratio failed: input not found', { ratio })
    }
  }

  private handleSendNuke(params?: unknown) {
    const nukeType = (params as any)?.nukeType

    if (!nukeType) {
      console.error('[GameBridge] send-nuke command missing nukeType parameter')
      this.ws.sendEvent('ERROR', 'send-nuke failed: missing nukeType', { params })
      return
    }

    // Map nukeType to unit type name (OpenFrontIO uses string names)
    let unitTypeName: string
    if (nukeType === 'atom') {
      unitTypeName = 'Atom Bomb'
    } else if (nukeType === 'hydro') {
      unitTypeName = 'Hydrogen Bomb'
    } else if (nukeType === 'mirv') {
      unitTypeName = 'MIRV'
    } else {
      console.error('[GameBridge] Invalid nuke type:', nukeType)
      this.ws.sendEvent('ERROR', 'send-nuke failed: invalid nuke type', { nukeType })
      return
    }

    console.log(`[GameBridge] Preparing to launch ${unitTypeName}`)

    try {
      // Get the build menu element to access the game instance and eventBus
      const buildMenu = document.querySelector('build-menu') as any
      if (!buildMenu || !buildMenu.game || !buildMenu.eventBus) {
        throw new Error('Build menu, game instance, or event bus not available')
      }

      const game = buildMenu.game
      const eventBus = buildMenu.eventBus

      // Get player to verify they can build
      const myPlayer = game.myPlayer()
      if (!myPlayer) {
        throw new Error('No player found - you may not have spawned yet')
      }

      console.log('[GameBridge] Getting buildable units for', unitTypeName)

      // Get all buildable units from player
      myPlayer.actions(null).then((actions: any) => {
        // Find the specific buildable unit by type name
        const buildableUnit = actions.buildableUnits.find((bu: any) => bu.type === unitTypeName)

        if (!buildableUnit) {
          console.error('[GameBridge] Buildable unit not found for type:', unitTypeName)
          console.log('[GameBridge] Available units:', actions.buildableUnits.map((bu: any) => bu.type))
          this.ws.sendEvent('ERROR', 'send-nuke failed: unit not available', { nukeType, unitTypeName })
          return
        }

        if (!buildableUnit.canBuild) {
          console.error('[GameBridge] Cannot build unit - no missile silo available or insufficient gold')
          this.ws.sendEvent('ERROR', 'send-nuke failed: cannot build (need missile silo and gold)', { nukeType })
          return
        }

        console.log('[GameBridge] Found buildable unit:', unitTypeName, 'can build from:', buildableUnit.canBuild)

        // Access the control panel to get UIState
        const controlPanel = document.querySelector('control-panel') as any
        if (!controlPanel || !controlPanel.uiState) {
          throw new Error('Control panel or UIState not available')
        }

        // Set ghostStructure to activate targeting mode - this is exactly what happens when you press keybinds 8/9/0
        // The game's StructureIconsLayer will show the ghost unit and handle the click to emit BuildUnitIntentEvent
        console.log('[GameBridge] 🎯 Activating ghost structure targeting mode for:', unitTypeName)
        controlPanel.uiState.ghostStructure = unitTypeName

        // Emit GhostStructureChangedEvent to update the UI and show the ghost
        class GhostStructureChangedEvent {
          constructor(public readonly ghostStructure: string | null) { }
        }
        eventBus.emit(new GhostStructureChangedEvent(unitTypeName))

        console.log('[GameBridge] ✓ Ghost targeting mode activated - click on the map to select target')

        this.ws.sendEvent('INFO', `${unitTypeName} targeting mode activated`, {
          nukeType,
          unitTypeName,
          message: 'Click on a tile on the map to target the nuke (ghost structure active)',
          timestamp: Date.now()
        })

        // Monitor for when the targeting completes (ghostStructure returns to null after click)
        const checkLaunchInterval = setInterval(() => {
          if (controlPanel.uiState.ghostStructure === null) {
            clearInterval(checkLaunchInterval)
            console.log('[GameBridge] ✓ Nuke targeting completed, command sent')

            this.ws.sendEvent('INFO', `${unitTypeName} launch command completed`, {
              nukeType,
              unitTypeName,
              timestamp: Date.now()
            })
          }
        }, 100)
      }).catch((err: Error) => {
        console.error('[GameBridge] Failed to get player actions:', err)
        this.ws.sendEvent('ERROR', 'send-nuke failed: could not get player actions', {
          nukeType,
          error: String(err)
        })
      })
    } catch (e) {
      console.error('[GameBridge] Failed to open build menu:', e)
      this.ws.sendEvent('ERROR', 'send-nuke failed: could not open build menu', {
        nukeType,
        error: String(e)
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
