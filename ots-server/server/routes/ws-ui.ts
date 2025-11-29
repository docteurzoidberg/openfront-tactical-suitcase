import { defineWebSocketHandler } from 'h3'
import type { OutgoingMessage } from '../../ots-shared/src/game'
import { getLatestState, hasActiveUserscriptPeers } from './ws-script'

export default defineWebSocketHandler({
  open(peer) {
    console.log('[ws-ui] open', peer.id)

    const latest = getLatestState()
    if (latest) {
      const msg = JSON.stringify({ type: 'state', payload: latest })
      peer.send(msg)
    }

    if (hasActiveUserscriptPeers()) {
      const info = JSON.stringify({
        type: 'event',
        payload: {
          type: 'INFO',
          timestamp: Date.now(),
          message: 'userscript-ws-open',
          data: { source: 'ws-ui-init' }
        }
      })
      peer.send(info)
    }

    peer.subscribe('ui')
  },

  close(peer) {
    console.log('[ws-ui] close', peer.id)
  },

  error(peer, error) {
    console.error('[ws-ui] error', peer.id, error)
  },

  message(peer, message) {
    const text = message.text()

    try {
      const parsed = JSON.parse(text) as OutgoingMessage
      if (parsed.type === 'cmd') {
        // Forward command to userscript peers
        peer.publish('script', text)

        // Build an INFO event so dashboards can log the command
        const info = JSON.stringify({
          type: 'event',
          payload: {
            type: 'INFO',
            timestamp: Date.now(),
            message: 'ui-cmd-sent',
            data: parsed.payload
          }
        })

        // Send back to this UI socket explicitly
        peer.send(info)

        // And broadcast to all other UI peers (if any)
        peer.publish('ui', info)
      }
    } catch {
      // ignore malformed
    }
  }
})
