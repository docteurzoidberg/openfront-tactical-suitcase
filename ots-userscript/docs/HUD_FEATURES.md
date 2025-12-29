# OTS Userscript HUD Features Documentation

## Overview

The OTS userscript provides a comprehensive **tabbed HUD interface** that overlays on the OpenFront.io game page. This document details all UI features, interactions, and technical implementation notes.

## HUD Architecture

### Window Structure

```
┌─────────────────────────────────────────┐
│ [WS ●] [GAME ●] [💓]     [⚙] [×]        │ ← Header
├─────────────────────────────────────────┤
│ [Logs] [Hardware] [Sound]               │ ← Tabs
├─────────────────────────────────────────┤
│                                         │
│          Tab Content Area               │
│                                         │
│                                         │
└─────────────────────────────────────────┘
                                     [⋰⋰] ← Resize handle
```

### Position & Sizing

- **Initial Position**: Top-right corner of viewport
- **Draggable**: Click and drag header bar to reposition
- **Resizable**: Drag bottom-right handle (min: 320×240px)
- **Persistence**: Position and size saved to `GM_setValue`
  - `ots-hud-pos`: {left, top}
  - `ots-hud-size`: {width, height}

## Header Components

### Status Pills

**WS Pill** (WebSocket Connection):
- `DISCONNECTED` — Red dot, not connected
- `CONNECTING` — Amber dot, attempting connection
- `OPEN` — Green dot, connected successfully
- `ERROR` — Red dot, connection error
- Updates immediately on connection state change

**GAME Pill** (Game State):
- Red dot — Not in game
- Green dot — Active game session
- Status derived from game API

### Heartbeat Indicator (💓)

- Blinks briefly when server heartbeat received (15s interval)
- Visual confirmation of active connection
- Uses CSS animation for pulse effect

### Gear Button (⚙)

- Opens settings panel for WebSocket URL configuration
- Settings panel floats independently above main HUD
- Can be dragged separately from main window

### Close Button (×)

- Hides HUD for current page lifetime
- Does not disconnect WebSocket
- HUD recreated on next log/status update

## Tab System

### Tab Navigation

Three tabs available:
1. **Logs** — Message stream and filtering
2. **Hardware** — Diagnostic display and controls
3. **Sound** — Audio event toggles and testing

**Behavior**:
- Click tab header to switch
- Active tab highlighted with blue background
- Tab state preserved during session (not persisted across reloads)
- Smooth transition between tabs

## Logs Tab

### Message Stream

**Log Entry Format**:
```
[HH:MM:SS] [TAG] Message content
```

**Tags**:
- `SEND` (blue) — Outgoing WebSocket messages
- `RECV` (green) — Incoming WebSocket messages
- `INFO` (yellow) — Internal system messages

**Auto-scroll**:
- Automatically scrolls to newest entry
- Smooth scrolling behavior
- Maximum 100 entries (older entries removed)

### Collapsible JSON

**Collapsed State**:
- Shows event type and summary message
- Click to expand full JSON payload
- Visual indicator: `▶` arrow

**Expanded State**:
- Color-formatted JSON with syntax highlighting:
  - **Keys**: Cyan (#00FFFF)
  - **Strings**: Green (#00FF00)
  - **Numbers**: Orange (#FFA500)
  - **Booleans**: Purple (#FF00FF)
  - **null**: Red (#FF0000)
- Click again to collapse
- Visual indicator: `▼` arrow
- Proper indentation (2 spaces)

**JSON Formatting Algorithm**:
- Recursive formatting preserves structure
- Handles nested objects and arrays
- Escapes special characters in strings
- Preserves type information (number vs string)

### Filter System

**Filter Controls**:
- Located above log area
- Two groups: Direction + Event Types

**Direction Filters**:
- ☑ Send — Show outgoing messages
- ☑ Recv — Show incoming messages
- ☑ Info — Show internal messages

**Event Type Filters**:
- ☑ Game Events — Start, end, win, lose
- ☑ Nukes — Nuclear launch events
- ☑ Alerts — Incoming threat alerts
- ☑ Troops — Troop count updates
- ☑ Sounds — Sound play events
- ☑ System — Hardware diagnostic/test events
- ☑ Heartbeat — Periodic connection pings (15s)

**Filter Behavior**:
- Real-time filtering (instant UI update)
- Multiple filters can be combined
- Unchecked filter hides matching entries
- Filter state persisted to `ots-log-filters` (GM storage)
- All filters enabled by default on first run

**Event Type Detection**:
```typescript
// Algorithm for event type classification
if (eventType === 'INFO') → Heartbeat filter
if (eventType.startsWith('ALERT_')) → Alerts filter
if (eventType.includes('LAUNCHED')) → Nukes filter
if (eventType === 'HARDWARE_DIAGNOSTIC') → System filter
if (eventType === 'SOUND_PLAY') → Sounds filter
// ... etc
```

## Hardware Tab

### Diagnostic Display

**Request Button**:
- Label: "🔍 Request Diagnostics"
- Sends `hardware-diagnostic` command to firmware
- Disabled while waiting for response
- Re-enables after 2 seconds

**Status Grid**:
Displays for each hardware module:
- **Module Name** (bold)
- **Type** (e.g., "alert", "nuke", "main_power")
- **Initialization**: ✓ OK (green) / ✗ FAILED (red)
- **Pin Configuration**: Board and pin numbers
- **I2C Address**: Hex format (e.g., "0x20")
- **Error State**: Error message if present

**Empty State**:
- Message: "No diagnostic data available"
- Shown when no response received yet
- Prompts user to click request button

**Data Update**:
- Auto-updates when `HARDWARE_DIAGNOSTIC` event received
- Captures events in `pushLog()` method
- Stores in `hardwareDiagnostics` property
- Re-renders grid on data change

## Sound Tab

### Sound Toggle Controls

**Available Sounds**:
1. `game_start` — "Game Start"
2. `game_player_death` — "Player Death"
3. `game_victory` — "Victory"
4. `game_defeat` — "Defeat"

**Toggle Row Format**:
```
☑ Game Start [game_start]  [▶ Test]
```

**Components per row**:
- **Checkbox**: Enable/disable sound event sending
- **Label**: Human-readable name
- **Code**: Sound ID in monospace font
- **Test Button**: Remote trigger button

**Toggle Behavior**:
- Checked: Send `SOUND_PLAY` events when game event occurs
- Unchecked: Suppress sound events (no message sent)
- State persisted to `ots-sound-toggles` (GM storage)
- Default: All sounds enabled on first run

### Remote Testing

**▶ Test Button**:
- Positioned at right end of each sound row
- Blue hover effect: rgba(59,130,246,0.3)
- Click sends `SOUND_PLAY` event immediately
- Respects toggle state (disabled if sound unchecked)

**Test Event Structure**:
```json
{
  "type": "event",
  "payload": {
    "type": "SOUND_PLAY",
    "timestamp": 1234567890,
    "message": "Test sound: game_start",
    "data": {
      "soundId": "game_start",
      "priority": "high",
      "test": true
    }
  }
}
```

**Test Behavior**:
- Logs to Info stream: "Testing sound: {soundId}"
- Sends via WebSocket with `test: true` flag
- Server/firmware play sound remotely
- No local playback in userscript
- Can test without triggering game events

## Settings Panel

### Access

- Click gear button (⚙) in header
- Floating panel appears above HUD
- Independent draggable window
- Does not affect main HUD position

### WebSocket URL Configuration

**Input Field**:
- Current WebSocket server URL
- Default: `ws://localhost:3000/ws`
- Placeholder text shows default
- Validates non-empty on save

**Reset Button**:
- Restores default URL in input field
- Does NOT immediately save or reconnect
- Must click Save to apply

**Save Button**:
- Validates URL is non-empty
- Persists to `ots-ws-url` (GM storage)
- Closes existing WebSocket connection (code 4100)
- Logs: "WS URL updated to {url}"
- Hides settings panel
- Initiates reconnection with new URL

**Close Button (×)**:
- Closes settings panel
- Discards unsaved changes
- Main HUD remains open

### Dragging

**Behavior**:
- Click and drag settings panel header
- Independent from main HUD
- Position NOT persisted (resets to top-right)
- Uses `FloatingPanel` class

## State Persistence

### GM Storage Keys

| Key | Type | Contents | Default |
|-----|------|----------|---------|
| `ots-hud-pos` | Object | {left, top} in pixels | {left: 20, top: 20} |
| `ots-hud-size` | Object | {width, height} in pixels | {width: 400, height: 500} |
| `ots-hud-collapsed` | Boolean | HUD visibility state | false |
| `ots-ws-url` | String | WebSocket server URL | "ws://localhost:3000/ws" |
| `ots-log-filters` | Object | Filter checkbox states | All true |
| `ots-sound-toggles` | Object | Sound toggle states | All true |

### Load Sequence

1. **Startup** (`main.user.ts`):
   - Read `ots-ws-url` → set WebSocket URL
   - Create `Hud` instance (lazy creation)

2. **HUD Creation** (`sidebar-hud.ts`):
   - Read `ots-hud-pos` → set window position
   - Read `ots-hud-size` → set window dimensions
   - Read `ots-log-filters` → initialize filter checkboxes
   - Read `ots-sound-toggles` → initialize sound toggles

3. **Auto-save**:
   - Position/size: On drag/resize end
   - Filters: On checkbox change
   - Sound toggles: On checkbox change
   - WS URL: On Save button click

## Integration Points

### WebSocket Client

**Callbacks to HUD**:
- `pushLog(direction, message, eventType)` — Add log entry
- `setStatus(status)` — Update WS pill
- `setGameStatus(status)` — Update GAME pill
- `onHeartbeat()` — Trigger heartbeat indicator blink

**Callbacks from HUD**:
- `getWsUrl()` — Retrieve current URL
- `setWsUrl(url)` — Update URL
- `onWsUrlChanged(url)` — Handle URL change
- `sendCommand(action, params)` — Send command to server
- `onSoundTest(soundId)` — Send test sound event

### Game Bridge

**Integration**:
- `hud.isSoundEnabled(soundId)` — Check if sound should send
- Called at 9 locations before sending `SOUND_PLAY` events:
  - Game start (1 location)
  - Player death (1 location)
  - Victory (4 locations: team win, solo win variants)
  - Defeat (3 locations: death, team loss, solo loss)

**Sound Event Flow**:
```
Game Event → openfront-bridge.ts
  ↓
isSoundEnabled(soundId)?
  ↓ (yes)
ws.sendEvent('SOUND_PLAY', ...)
  ↓
WebSocket → Server/Firmware → Play sound
```

## Visual Design

### Color Scheme

**Status Colors**:
- Green (#00FF00) — Connected, OK
- Amber (#FFA500) — Connecting, Warning
- Red (#FF0000) — Error, Disconnected
- Blue (#3B82F6) — Sent messages, Active tab

**Tag Colors**:
- SEND: Blue (rgba(59, 130, 246, 0.2))
- RECV: Green (rgba(16, 185, 129, 0.2))
- INFO: Yellow (rgba(245, 158, 11, 0.2))

**Syntax Highlighting**:
- Keys: Cyan (#00FFFF)
- Strings: Green (#00FF00)
- Numbers: Orange (#FFA500)
- Booleans: Purple (#FF00FF)
- null: Red (#FF0000)

### Typography

- **Headers**: 14px bold
- **Body**: 12px monospace (Consolas, Monaco)
- **Logs**: 11px monospace
- **JSON**: 11px monospace with indentation

### Spacing

- **Padding**: 8px-12px on containers
- **Margins**: 4px-8px between elements
- **Tab padding**: 8px horizontal, 6px vertical
- **Filter gaps**: 8px between checkboxes

### Animations

- **Heartbeat blink**: 300ms pulse on indicator
- **Tab switching**: Instant (no animation)
- **Hover effects**: 200ms transition on buttons
- **JSON expand/collapse**: Instant (no animation)

## Performance Notes

### Log Management

- **Max entries**: 100 logs stored
- **Overflow**: Oldest entries removed (FIFO)
- **Filtering**: O(n) scan on each log add (acceptable for 100 entries)
- **Rendering**: Direct DOM manipulation (no virtual DOM)

### JSON Formatting

- **Algorithm**: Recursive with memoization
- **Max depth**: No limit (handles deep nesting)
- **Performance**: Fast for typical game events (<1ms)
- **Memory**: One formatted string per collapsed log

### Event Detection

- **Polling rate**: 100ms (game trackers)
- **Sound checks**: 9 locations, minimal overhead
- **Filter checks**: Per-log, cached filter state

## Troubleshooting

### HUD Not Appearing

1. Check browser console for errors
2. Verify Tampermonkey is active
3. Confirm script matches `https://openfront.io/*`
4. Try refreshing page
5. Check if HUD was closed (×) — will reappear on next event

### WebSocket Not Connecting

1. Check WS URL in settings (gear icon)
2. Verify server is running on configured port
3. Check firewall not blocking connection
4. Look for connection errors in Logs tab (Info messages)

### Sounds Not Playing

1. Verify sound toggles are checked (Sound tab)
2. Confirm server/firmware is connected
3. Check server console for SOUND_PLAY events
4. Use ▶ Test buttons to test without game events
5. Remember: userscript does NOT play sounds locally

### Filters Not Working

1. Check filter states are persisted (GM storage)
2. Verify event type detection logic
3. Look for filter state in browser DevTools
4. Reset filters by unchecking/rechecking all

### Hardware Diagnostics Empty

1. Click "🔍 Request Diagnostics" button
2. Verify firmware is connected and running
3. Check firmware serial output for diagnostic response
4. Look for `HARDWARE_DIAGNOSTIC` event in logs
5. Confirm event data capture in `pushLog()` method

## Future Enhancements

### Planned Features

- [ ] Volume control for sound tests
- [ ] "Test All" button for sounds
- [ ] Filter presets (save/load combinations)
- [ ] Export logs to file
- [ ] Collapse/expand all JSON button
- [ ] Search/filter in log history
- [ ] Custom sound IDs (user-defined)
- [ ] Hardware module control (not just diagnostics)
- [ ] Themes (dark/light mode)
- [ ] Keyboard shortcuts for tab switching

### Technical Improvements

- [ ] Virtual scrolling for large log counts
- [ ] Web Workers for JSON formatting
- [ ] IndexedDB for log persistence
- [ ] Lazy loading of collapsed JSON
- [ ] Debounced filter updates
- [ ] Optimized event type detection (Map/Set)

## References

- Main context: `/ots-userscript/copilot-project-context.md`
- Protocol spec: `/prompts/protocol-context.md`
- Shared types: `/ots-shared/src/game.ts`
- Source code: `/ots-userscript/src/hud/sidebar-hud.ts`

---

**Last Updated**: December 29, 2025
**Version**: 2025-12-20.1
