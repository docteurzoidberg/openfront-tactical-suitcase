# WebSocket Protocol - Developer Guide

This guide explains how to implement and work with the OTS WebSocket protocol across all components.

**Protocol Specification:** [`prompts/WEBSOCKET_MESSAGE_SPEC.md`](../../prompts/WEBSOCKET_MESSAGE_SPEC.md) (single source of truth)

---

## Overview

The OTS WebSocket protocol enables real-time communication between:
- **Userscript** (ots-userscript) - Detects game events, sends to server
- **Dashboard** (ots-simulator) - UI controls, displays game state
- **Firmware** (ots-fw-main) - Physical hardware control

All components connect to a single WebSocket endpoint (`/ws`) and identify themselves via handshake messages.

### Architecture

```
Game (openfront.io)
  ↓ (polling 100ms)
Userscript
  ↓ (WebSocket)
Server (ots-simulator)
  ↓ (broadcast)
├─→ Dashboard UI (ots-simulator)
└─→ Firmware (ots-fw-main)
```

**Key Characteristics:**
- **Event-driven**: All state changes trigger events
- **Broadcast model**: Server relays all messages to all connected clients
- **Client identification**: Handshake message with `clientType` field
- **JSON over WebSocket**: UTF-8 text frames only

---

## Getting Started

### 1. Establishing Connection

**Client-side (TypeScript):**
```typescript
import { useWebSocket } from '@vueuse/core'

const { status, data, send, open, close } = useWebSocket('ws://localhost:3000/ws', {
  autoReconnect: true,
  heartbeat: {
    message: JSON.stringify({
      type: 'event',
      payload: { type: 'INFO', timestamp: Date.now(), message: 'heartbeat' }
    }),
    interval: 15000
  },
  onConnected: () => {
    // Send handshake
    send(JSON.stringify({
      type: 'handshake',
      clientType: 'ui'  // or 'userscript', 'firmware'
    }))
  }
})
```

**Server-side (Nitro WebSocket handler):**
```typescript
// server/routes/ws.ts
export default defineWebSocketHandler({
  open(peer) {
    console.log('[ws] Client connected')
  },
  
  message(peer, message) {
    const msg = JSON.parse(message.text())
    
    if (msg.type === 'handshake') {
      peer.subscribe('broadcast')
      peer.subscribe(msg.clientType)  // 'ui', 'userscript', or 'firmware'
      peer.send({ type: 'handshake-ack', clientType: msg.clientType })
    }
    
    // Broadcast all messages
    peer.publish('broadcast', message.text())
  }
})
```

### 2. Sending Events

**From Userscript (game event detection):**
```typescript
function sendGameEvent(type: GameEventType, data?: any) {
  const event: GameEvent = {
    type,
    timestamp: Date.now(),
    message: `Event: ${type}`,
    data
  }
  
  wsClient.send(JSON.stringify({
    type: 'event',
    payload: event
  }))
}

// Example: Nuclear launch detected
sendGameEvent('NUKE_LAUNCHED', {
  nukeType: 'Atom Bomb',
  nukeUnitID: 12345,  // CRITICAL: Required for tracking
  targetTile: 54321,
  targetPlayerID: currentPlayer.id
})
```

**From Firmware (hardware event):**
```c
void send_game_event(game_event_type_t event_type, const char* data_json) {
  cJSON* root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "type", "event");
  
  cJSON* payload = cJSON_CreateObject();
  cJSON_AddStringToObject(payload, "type", game_event_type_to_string(event_type));
  cJSON_AddNumberToObject(payload, "timestamp", esp_timer_get_time() / 1000);
  
  if (data_json && strlen(data_json) > 0) {
    cJSON* data = cJSON_Parse(data_json);
    if (data) {
      cJSON_AddItemToObject(payload, "data", data);
    }
  }
  
  cJSON_AddItemToObject(root, "payload", payload);
  
  char* json_str = cJSON_PrintUnformatted(root);
  ws_server_send_text(json_str, strlen(json_str));
  
  free(json_str);
  cJSON_Delete(root);
}
```

### 3. Handling Commands

**Dashboard sending command:**
```typescript
function sendNuke(nukeType: 'atom' | 'hydro' | 'mirv') {
  wsClient.send(JSON.stringify({
    type: 'cmd',
    payload: {
      action: 'send-nuke',
      params: { nukeType }
    }
  }))
}
```

**Userscript handling command:**
```typescript
wsClient.on('message', (msg: IncomingMessage) => {
  if (msg.type === 'cmd') {
    const { action, params } = msg.payload
    
    switch (action) {
      case 'send-nuke':
        handleSendNuke(params.nukeType)
        break
      case 'set-troops-percent':
        handleSetTroopsPercent(params.percent)
        break
      case 'ping':
        sendGameEvent('INFO', { message: 'pong-from-userscript' })
        break
    }
  }
})
```

---

## Common Patterns

### Event Detection (Userscript)

The userscript uses polling (100ms interval) to detect game state changes:

```typescript
class NukeTracker {
  private tracked = new Map<number, TrackedNuke>()
  
  update() {
    const nukes = game.units().filter(u => u.type().startsWith('Nuke'))
    
    for (const nuke of nukes) {
      const unitID = nuke.id()
      
      if (!this.tracked.has(unitID)) {
        // New nuke detected - emit NUKE_LAUNCHED event
        this.tracked.set(unitID, {
          unitID,
          type: nuke.type(),
          targetTile: nuke.targetTile()
        })
        
        sendGameEvent('NUKE_LAUNCHED', {
          nukeType: nuke.type(),
          nukeUnitID: unitID,
          targetTile: nuke.targetTile(),
          // ... other data
        })
      }
      
      // Check for outcome (exploded or intercepted)
      if (nuke.hasArrived()) {
        sendGameEvent('NUKE_EXPLODED', { unitID })
        this.tracked.delete(unitID)
      }
    }
    
    // Check for deleted units (intercepted)
    for (const [unitID, nuke] of this.tracked) {
      if (!game.getUnit(unitID)) {
        sendGameEvent('NUKE_INTERCEPTED', { unitID })
        this.tracked.delete(unitID)
      }
    }
  }
}

setInterval(() => nukeTracker.update(), 100)
```

**Critical Requirements:**
- Always include `nukeUnitID` in launch events
- Track by unit ID (not by timeout)
- Emit outcome events (`NUKE_EXPLODED` or `NUKE_INTERCEPTED`)

### Hardware State Tracking (Firmware)

Firmware uses count-based state machines (not timers) for LED control:

```c
// ❌ BAD: Timer-based (breaks with multiple nukes)
void handle_nuke_launch() {
  gpio_set_level(LED_PIN, 1);
  start_timer(10000);  // Turn off after 10s
}

// ✅ GOOD: Count-based state tracking
void alert_module_handle_event(game_event_type_t event, const char* data) {
  if (event == GAME_EVENT_ALERT_ATOM) {
    uint32_t unit_id = parse_nuke_unit_id(data);
    alert_tracker_register(NUKE_TYPE_ATOM, unit_id, NUKE_DIR_INCOMING);
    
    // Turn LED ON if any nukes tracked
    if (alert_tracker_get_count(NUKE_TYPE_ATOM, NUKE_DIR_INCOMING) > 0) {
      gpio_set_level(ALERT_LED_ATOM_PIN, 1);
    }
  }
  
  if (event == GAME_EVENT_NUKE_EXPLODED || event == GAME_EVENT_NUKE_INTERCEPTED) {
    uint32_t unit_id = parse_unit_id(data);
    alert_tracker_unregister(NUKE_TYPE_ATOM, unit_id, NUKE_DIR_INCOMING);
    
    // Turn LED OFF only when count reaches 0
    if (alert_tracker_get_count(NUKE_TYPE_ATOM, NUKE_DIR_INCOMING) == 0) {
      gpio_set_level(ALERT_LED_ATOM_PIN, 0);
    }
  }
}
```

**Key Principles:**
- Track nukes by `unitID` (max 32 simultaneous)
- LED state depends on active count, not timers
- Support multiple simultaneous nukes per type
- Separate tracking for outgoing (Nuke Module) and incoming (Alert Module)

### Command Throttling (Troops Slider)

Prevent command spam by debouncing and threshold checking:

```typescript
let lastSentPercent = 0
let lastReadTime = 0

function pollSlider() {
  const now = Date.now()
  
  // Debounce: Read every 100ms
  if (now - lastReadTime < 100) return
  lastReadTime = now
  
  const adcValue = readADC()
  const percent = Math.round((adcValue / 4095) * 100)
  
  // Threshold: Send only on ≥1% change
  if (Math.abs(percent - lastSentPercent) >= 1) {
    wsClient.send(JSON.stringify({
      type: 'cmd',
      payload: {
        action: 'set-troops-percent',
        params: { percent }
      }
    }))
    
    lastSentPercent = percent
  }
}

setInterval(pollSlider, 100)
```

---

## Debugging

### WebSocket Inspector (Browser)

1. Open DevTools → Network tab
2. Filter by "WS" (WebSocket)
3. Click WebSocket connection
4. View Messages tab

**What to look for:**
- Handshake messages (should be first)
- Event flow: Userscript → Server → Dashboard
- Command flow: Dashboard → Server → Userscript
- Heartbeat events (every 15 seconds)

### Firmware Serial Logs

```c
void ws_handle_message(const char* data) {
  ESP_LOGI(TAG, "Received: %s", data);
  
  cJSON* root = cJSON_Parse(data);
  if (!root) {
    ESP_LOGE(TAG, "Failed to parse JSON");
    return;
  }
  
  const char* type = cJSON_GetObjectItem(root, "type")->valuestring;
  ESP_LOGI(TAG, "Message type: %s", type);
  
  // ... process message
  
  cJSON_Delete(root);
}
```

### Common Issues

**Issue: LEDs stay on after nuke resolves**
- Check: Outcome events (`NUKE_EXPLODED` or `NUKE_INTERCEPTED`) received?
- Check: `unitID` parsing correct?
- Check: Tracker count reaches 0?
- Fix: Add logging to track register/unregister operations

**Issue: Multiple nukes break LED behavior**
- Check: Using count-based tracking (not timers)?
- Check: Each nuke has unique `unitID`?
- Fix: Implement proper state tracker (see example above)

**Issue: Commands not reaching userscript**
- Check: Handshake completed?
- Check: Client subscribed to `broadcast` channel?
- Check: Userscript has command handler?
- Fix: Check server broadcast logic, verify client subscription

**Issue: Slider spamming commands**
- Check: Debouncing implemented (100ms minimum)?
- Check: Threshold check (≥1% change)?
- Fix: Add throttling logic (see example above)

---

## Testing

### Manual Testing

**1. WebSocket Connection:**
```bash
# Install wscat
npm install -g wscat

# Connect to server
wscat -c ws://localhost:3000/ws

# Send handshake
> {"type":"handshake","clientType":"ui"}

# Should receive handshake-ack
< {"type":"handshake-ack","clientType":"ui"}
```

**2. Send Test Event:**
```bash
> {"type":"event","payload":{"type":"GAME_START","timestamp":1234567890}}
```

**3. Send Test Command:**
```bash
> {"type":"cmd","payload":{"action":"ping"}}
```

### Automated Testing

**Python WebSocket Test Script:**
```python
import asyncio
import websockets
import json

async def test_protocol():
    uri = "ws://localhost:3000/ws"
    async with websockets.connect(uri) as websocket:
        # Handshake
        await websocket.send(json.dumps({
            "type": "handshake",
            "clientType": "ui"
        }))
        
        response = await websocket.recv()
        print(f"Handshake: {response}")
        
        # Send event
        await websocket.send(json.dumps({
            "type": "event",
            "payload": {
                "type": "NUKE_LAUNCHED",
                "timestamp": 1234567890,
                "data": {"nukeUnitID": 12345}
            }
        }))
        
        # Wait for broadcast
        response = await websocket.recv()
        print(f"Broadcast: {response}")

asyncio.run(test_protocol())
```

---

## Best Practices

### Protocol Changes

**Always follow this order:**
1. Update [`prompts/WEBSOCKET_MESSAGE_SPEC.md`](../../prompts/WEBSOCKET_MESSAGE_SPEC.md)
2. Update TypeScript types in `ots-shared/src/game.ts`
3. Update C types in `ots-fw-main/include/protocol.h`
4. Update implementations (userscript, simulator, firmware)
5. Test end-to-end

**Example commit sequence:**
```bash
# Commit 1: Protocol spec
git add prompts/WEBSOCKET_MESSAGE_SPEC.md
git commit -m "protocol: add TROOPS_UPDATED event"

# Commit 2: Shared types
git add ots-shared/
git commit -m "shared: add TROOPS_UPDATED event type"

# Commit 3: Firmware protocol
git add ots-fw-main/include/protocol.h ots-fw-main/src/protocol.c
git commit -m "firmware: add TROOPS_UPDATED event type"

# Commit 4: Implementations
git add ots-userscript/ ots-simulator/ ots-fw-main/
git commit -m "feat: implement TROOPS_UPDATED event tracking"
```

### Error Handling

**Always validate message structure:**
```typescript
function handleMessage(msg: unknown) {
  if (typeof msg !== 'object' || !msg) {
    console.error('Invalid message: not an object')
    return
  }
  
  if (!('type' in msg)) {
    console.error('Invalid message: missing type field')
    return
  }
  
  switch (msg.type) {
    case 'event':
      if (!('payload' in msg) || typeof msg.payload !== 'object') {
        console.error('Invalid event: missing/invalid payload')
        return
      }
      handleEvent(msg.payload as GameEvent)
      break
    // ... other types
  }
}
```

### Performance

**Minimize message frequency:**
- ✅ Debounce slider updates (100ms)
- ✅ Threshold check before sending (≥1% change)
- ✅ Batch state updates when possible
- ❌ Don't send unchanged state repeatedly
- ❌ Don't poll faster than necessary (100ms is sufficient)

**Optimize JSON payload size:**
- ✅ Use short field names
- ✅ Omit null/undefined fields
- ✅ Use numeric IDs instead of strings
- ❌ Don't include unnecessary metadata

---

## Reference Implementation

### Complete Userscript Example

See: `ots-userscript/src/websocket/client.ts`

Key features:
- Auto-reconnect with exponential backoff
- Handshake on connect
- Event detection via game polling
- Command handling with proper validation

### Complete Server Example

See: `ots-simulator/server/routes/ws.ts`

Key features:
- Single endpoint with client identification
- Broadcast to all connected peers
- Channel-based pub/sub (broadcast, ui, userscript)
- Connection status events

### Complete Firmware Example

See: `ots-fw-main/src/ws_server.c`

Key features:
- cJSON for parsing/serialization
- Event dispatcher routing to modules
- State tracking with unit IDs
- LED control based on tracker counts

---

## Further Reading

- **Protocol Specification**: [`prompts/WEBSOCKET_MESSAGE_SPEC.md`](../../prompts/WEBSOCKET_MESSAGE_SPEC.md)
- **Userscript Context**: [`ots-userscript/copilot-project-context.md`](../../ots-userscript/copilot-project-context.md)
- **Simulator Context**: [`ots-simulator/copilot-project-context.md`](../../ots-simulator/copilot-project-context.md)
- **Firmware Context**: [`ots-fw-main/copilot-project-context.md`](../../ots-fw-main/copilot-project-context.md)
- **Git Workflow**: [`prompts/GIT_WORKFLOW.md`](../../prompts/GIT_WORKFLOW.md)
- **Hardware Specs**: [`ots-hardware/README.md`](../../ots-hardware/README.md)
