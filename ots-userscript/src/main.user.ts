import { Hud } from './hud/main-hud'
import { WsClient } from './websocket/client'
import { GameBridge } from './game/openfront-bridge'
import { loadWsUrl, saveWsUrl } from './storage/config'

  ; (function start() {
    let currentWsUrl = loadWsUrl()

    const hud = new Hud(
      () => currentWsUrl,
      (url) => {
        currentWsUrl = url
        saveWsUrl(url)
      },
      () => {
        ws.disconnect(4100, 'URL changed')
        ws.connect()
      }
    )

    // Create game bridge first (needed for command handling)
    let game: GameBridge | null = null

    // Create WebSocket client with command handler
    const ws = new WsClient(
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

    // Start everything
    hud.ensure()
    ws.connect()
    game.init()

    // Expose for debugging
      ; (window as any).otsShowHud = () => hud.ensure()
      ; (window as any).otsWsClient = ws
      ; (window as any).otsGameBridge = game
  })()
