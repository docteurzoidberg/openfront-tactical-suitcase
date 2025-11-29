import type { WsClient } from '../websocket/client'
import type { Hud } from '../hud/main-hud'
import { waitForElement, waitForGameInstance } from '../utils/dom'
import { debugLog } from '../utils/logger'

const CONTROL_PANEL_SELECTOR = 'control-panel'
const GAME_UPDATE_EVENT = 'openfront:game-update'
const GAME_UPDATE_TYPE_UNIT = 1

let gameUpdateListenerAttached = false
let initialPayloadSent = false

function hookGameUpdates(game: any) {
  if (!game) {
    debugLog('[OTS] No GameView instance to hook.')
    return
  }

  const proto = Object.getPrototypeOf(game)
  if (!proto || typeof proto.update !== 'function') {
    debugLog('[OTS] GameView prototype missing update method; skipping hook.')
    return
  }

  if (proto.__openfrontGameHooked) {
    return
  }

  const originalUpdate = proto.update
  proto.update = function patchedGameUpdate(this: any, ...args: any[]) {
    const result = originalUpdate.apply(this, args)
    window.dispatchEvent(
      new CustomEvent(GAME_UPDATE_EVENT, {
        detail: { game: this, payload: args[0] }
      })
    )
    return result
  }

  Object.defineProperty(proto, '__openfrontGameHooked', {
    value: true,
    configurable: true
  })

  debugLog('[OTS] Hooked GameView.update().')
  attachGameUpdateListener()
}

function attachGameUpdateListener() {
  if (gameUpdateListenerAttached) {
    return
  }

  window.addEventListener(GAME_UPDATE_EVENT, handleGameUpdate as EventListener)
  gameUpdateListenerAttached = true
}

function handleGameUpdate(event: CustomEvent<{ game: any; payload: any }>) {
  const game = event?.detail?.game
  const viewData = event?.detail?.payload
  if (!game || !viewData || !viewData.updates) {
    return
  }

  // On first update, send the full raw payload once so the
  // server/dashboard can inspect the structure.
  const ws = (window as any).otsWsClient as WsClient | undefined
  if (!ws) {
    return
  }

  if (!initialPayloadSent) {
    ws.sendEvent('INFO', 'of-initial-gameview-payload', {
      game,
      payload: viewData
    })
    initialPayloadSent = true
  }

  const myPlayer = typeof game.myPlayer === 'function' ? game.myPlayer() : null
  const mySmallID =
    typeof myPlayer?.smallID === 'function' ? myPlayer.smallID() : undefined
  if (mySmallID === undefined) {
    return
  }

  const unitUpdates = viewData.updates[GAME_UPDATE_TYPE_UNIT] ?? []
  const myUnits = unitUpdates.filter((u: any) => u.ownerID === mySmallID)

  if (!myUnits.length) return

  ws.sendEvent('INFO', 'of-game-unit-updates', {
    count: myUnits.length,
    sample: myUnits.slice(0, 3).map((u: any) => ({
      id: u.id,
      type: u.unitType,
      ownerID: u.ownerID,
      troops: u.troops,
      underConstruction: u.underConstruction,
      isActive: u.isActive
    }))
  })
}

export class GameBridge {
  constructor(private ws: WsClient, private hud: Hud) { }

  init() {
    waitForElement(CONTROL_PANEL_SELECTOR, (controlPanel) => {
      debugLog('[OTS] control panel detected', controlPanel)
      waitForGameInstance(controlPanel, (game) => {
        ; (window as any).openfrontControlPanelGame = game
        debugLog('[OTS] GameView ready', game)
        this.ws.sendEvent('INFO', 'of-game-instance-detected', game)
        this.hud.setGameStatus(true)
        hookGameUpdates(game)
      })
    })
  }
}
