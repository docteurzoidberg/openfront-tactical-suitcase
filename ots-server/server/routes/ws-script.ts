import { defineWebSocketHandler } from 'h3'
import type { GameState, IncomingMessage } from '../../ots-shared/src/game'

let latestState: GameState | null = null
const activeUserscriptPeers = new Set<string>()

export default defineWebSocketHandler({
  open(peer) {
    console.log('[ws-script] open', peer.id)
    activeUserscriptPeers.add(peer.id)
    peer.subscribe('script')
    const msg = JSON.stringify({
      type: 'event',
      payload: {
        type: 'INFO',
        timestamp: Date.now(),
        message: 'userscript-ws-open',
        data: { peerId: peer.id }
      }
    })
    peer.publish('ui', msg)
  },

  close(peer) {
    console.log('[ws-script] close', peer.id)
    activeUserscriptPeers.delete(peer.id)
    const msg = JSON.stringify({
      type: 'event',
      payload: {
        type: 'INFO',
        timestamp: Date.now(),
        message: 'userscript-ws-close',
        data: { peerId: peer.id }
      }
    })
    peer.publish('ui', msg)
  },

  error(peer, error) {
    console.error('[ws-script] error', peer.id, error)
  },

  message(peer, message) {
    const text = message.text()

    try {
      const parsed = JSON.parse(text) as IncomingMessage

      if (parsed.type === 'state') {
        latestState = parsed.payload
      }

      peer.publish('ui', text)
    } catch (err) {
      console.error('[ws-script] failed to parse message', err)
    }
  }
})

export function getLatestState() {
  return latestState
}

export function hasActiveUserscriptPeers() {
  return activeUserscriptPeers.size > 0
}
