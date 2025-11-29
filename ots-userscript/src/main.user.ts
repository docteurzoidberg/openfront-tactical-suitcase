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

  const ws = new WsClient(hud, () => currentWsUrl)
  const game = new GameBridge(ws, hud)

  hud.ensure()
  ws.connect()
  game.init()
    ; (window as any).otsShowHud = () => hud.ensure()
    ; (window as any).otsWsClient = ws
})()
