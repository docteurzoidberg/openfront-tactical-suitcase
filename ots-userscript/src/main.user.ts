import { Hud } from './hud/sidebar-hud'
import { WsClient } from './websocket/client'
import { GameBridge, WS_CLOSE_CODE_URL_CHANGED } from './game'
import { loadWsUrl, saveWsUrl } from './storage/config'

// Version (updated by release.sh)
const VERSION = '2025-12-29.1'

  ; (function start() {
    console.log(`[OTS Userscript] Version ${VERSION}`)
    let currentWsUrl = loadWsUrl()

    // Create WebSocket client early (will be initialized later)
    let ws: WsClient

    const hud = new Hud(
      () => currentWsUrl,
      (url) => {
        currentWsUrl = url
        saveWsUrl(url)
      },
      () => {
        ws.disconnect(WS_CLOSE_CODE_URL_CHANGED, 'URL changed')
        ws.connect()
      },
      (action, params) => {
        // Send command via WebSocket
        if (ws) {
          ws.sendCommand(action, params)
        }
      },
      (soundId) => {
        // Test sound by sending SOUND_PLAY event
        if (ws) {
          ws.sendEvent('SOUND_PLAY', `Test sound: ${soundId}`, {
            soundId,
            priority: 'high',
            test: true
          })
        }
      }
    )

    // Create game bridge first (needed for command handling)
    let game: GameBridge | null = null

    // Create WebSocket client with command handler
    ws = new WsClient(
      hud,
      () => currentWsUrl,
      (action, params) => {
        if (game) {
          game.handleCommand(action, params)
        }
      }
    )

    // Create game bridge
    game = new GameBridge(ws, hud)

    // Wait for page to be ready before connecting
    function initialize() {
      console.log('[OTS Userscript] Initializing...')
      hud.ensure()
      ws.connect()
      game!.init()

        // Expose for debugging
        ; (window as any).otsShowHud = () => hud.ensure()
        ; (window as any).otsWsClient = ws
        ; (window as any).otsGameBridge = game
    }

    // Start when DOM is ready
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', initialize)
    } else {
      // DOM already loaded
      initialize()
    }
  })()
