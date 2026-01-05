import { defineWebSocketHandler } from 'h3'
import type { IncomingMessage, OutgoingMessage } from '../../../ots-shared/src/game'
import { PROTOCOL_CONSTANTS } from '../../../ots-shared/src/game'

// Track peer types using a Map
const peerTypes = new Map<string, 'ui' | 'userscript' | 'unknown'>()

export default defineWebSocketHandler({
  async open(peer) {
    // Default to userscript unless identified as UI via handshake
    peerTypes.set(peer.id, 'userscript')
    console.log(`[ws] open ${peer.id} (default: userscript)`)

    // Subscribe to broadcast and userscript channels by default
    peer.subscribe('broadcast')
    peer.subscribe('userscript')
  },

  close(peer) {
    const clientType = peerTypes.get(peer.id)
    console.log('[ws] close', peer.id, `(type: ${clientType})`)

    // If a userscript disconnects, notify all UI clients
    if (clientType === 'userscript') {
      const disconnectEvent = JSON.stringify({
        type: 'event',
        payload: {
          type: 'INFO',
          timestamp: Date.now(),
          message: PROTOCOL_CONSTANTS.INFO_MESSAGE_USERSCRIPT_DISCONNECTED,
          data: { peerId: peer.id }
        }
      })
      peer.publish('broadcast', disconnectEvent)
    }

    // Clean up peer type tracking
    peerTypes.delete(peer.id)
  },

  error(peer, error) {
    console.error('[ws] error', peer.id, error)
  },

  message(peer, message) {
    const text = message.text()

    try {
      const parsed = JSON.parse(text)

      // Check for handshake message to identify client type
      if (parsed.type === 'handshake' && parsed.clientType) {
        const clientType = parsed.clientType === PROTOCOL_CONSTANTS.CLIENT_TYPE_UI ? 'ui' : 'userscript'

        // Only update if changing from default or if it's UI
        if (clientType === 'ui') {
          peerTypes.set(peer.id, clientType)
          console.log(`[ws] handshake ${peer.id} identified as ${clientType}`)

          // UI clients need to subscribe to UI channel and unsubscribe from userscript
          peer.subscribe('ui')
          peer.unsubscribe('userscript')
        } else {
          console.log(`[ws] handshake ${peer.id} confirmed as ${clientType}`)
        }

        // Send acknowledgment
        peer.send(JSON.stringify({ type: 'handshake-ack', clientType }))

        // If a userscript connects, notify all UI clients
        if (clientType === 'userscript') {
          const connectEvent = JSON.stringify({
            type: 'event',
            payload: {
              type: 'INFO',
              timestamp: Date.now(),
              message: PROTOCOL_CONSTANTS.INFO_MESSAGE_USERSCRIPT_CONNECTED,
              data: { peerId: peer.id }
            }
          })
          peer.publish('broadcast', connectEvent)
        }

        return
      }

      // Check if it's an incoming message (event from userscript)
      if (parsed.type === 'event') {
        const msg = parsed as IncomingMessage
        // Broadcast event to all connected peers (UI and other userscripts)
        peer.publish('broadcast', text)
      }
      // Check if it's an outgoing message (cmd from UI)
      else if (parsed.type === 'cmd') {
        const msg = parsed as OutgoingMessage

        // Handle hardware-diagnostic command locally (simulator response)
        if (msg.type === 'cmd' && msg.payload.action === 'hardware-diagnostic') {
          const diagnostic = JSON.stringify({
            type: 'event',
            payload: {
              type: 'HARDWARE_DIAGNOSTIC',
              timestamp: Date.now(),
              message: 'OTS Simulator Diagnostic',
              data: {
                version: '2025-12-20.1', // Should match package.json version
                deviceType: 'simulator',
                serialNumber: 'OTS-SIM-000001',
                owner: 'PUSH Team',
                hardware: {
                  lcd: { present: true, working: true },
                  inputBoard: { present: true, working: true },
                  outputBoard: { present: true, working: true },
                  adc: { present: true, working: true },
                  soundModule: { present: true, working: true }
                }
              }
            }
          })
          // Send diagnostic response back to requester and broadcast
          peer.send(diagnostic)
          peer.publish('broadcast', diagnostic)
          return
        }

        // Broadcast command to all peers (userscript will receive it)
        peer.publish('broadcast', text)

        // Also send an INFO event back to log the command
        if (msg.type === 'cmd') {
          const info = JSON.stringify({
            type: 'event',
            payload: {
              type: 'INFO',
              timestamp: Date.now(),
              message: `Command: ${msg.payload.action}`,
              data: msg.payload
            }
          })
          peer.publish('broadcast', info)
        }
      }
    } catch (err) {
      console.error('[ws] failed to parse message', err)
    }
  }
})
