# Sidebar HUD Design Documentation

## Overview

The Sidebar HUD is a **tabbed, draggable, and resizable** interface component that provides:
- Real-time WebSocket connection monitoring
- Comprehensive event logging with advanced filtering
- Hardware diagnostic display and control
- Sound event management with remote testing
- Settings panel for WebSocket configuration

**Current Implementation**: Fully featured with 3 tabs (Logs, Hardware, Sound), collapsible JSON logs, sound testing, and hardware diagnostics.

## Key Features

### 1. Tabbed Interface

#### Tab Structure
- **3 Tabs**: Logs, Hardware, Sound
- **Navigation**: Click tab header to switch
- **Visual Feedback**: Active tab highlighted with blue background
- **State**: Tab selection preserved during session (not across reloads)

#### Tab Content
- **Logs**: Message stream with filtering and collapsible JSON
- **Hardware**: Diagnostic display with request button
- **Sound**: Toggle controls with remote test buttons

### 2. Positioning & Window Management

#### Fixed Position
- **Location**: Top-right corner of viewport
- **Draggable**: Click and drag header bar to reposition anywhere
- **Persistence**: Position saved to `ots-hud-pos` (GM storage)
- **Restore**: Position restored on page reload

#### Resizing
- **Handle**: Bottom-right corner with diagonal grip lines (⋰⋰)
- **Minimum Size**: 320px × 240px
- **Persistence**: Size saved to `ots-hud-size` (GM storage)
- **Restore**: Dimensions restored on page reload

### 3. Header Components
- **Size**: 20x20px
- **States**:
  - Default opacity: 0.5
  - Hover opacity: 1.0
  - Hidden when collapsed

#### Status Pills
- **WS Pill**: WebSocket connection status with colored dot
  - `DISCONNECTED` — Red (#f97373)
  - `CONNECTING` — Amber (#facc15)
  - `OPEN` — Green (#4ade80)
  - `ERROR` — Red (#f97373)
- **GAME Pill**: Game connection status (green/red)

#### Heartbeat Indicator (💓)
- **Purpose**: Visual confirmation of active connection
- **Behavior**: Blinks on server heartbeat (15s interval)
- **Animation**: CSS pulse effect (300ms)

#### Control Buttons
- **Gear (⚙)**: Opens settings panel for WebSocket URL
- **Close (×)**: Hides HUD for current session

### 4. Logs Tab Features

#### Message Stream
- **Display**: Rolling log of WebSocket messages
- **Tags**: Color-coded message direction
  - `SEND` (blue) — Outgoing to server
  - `RECV` (green) — Incoming from server
  - `INFO` (yellow) — Internal messages
- **Auto-scroll**: Automatic scroll to newest entry
- **Limit**: Maximum 100 entries (FIFO removal)

#### Collapsible JSON
- **Collapsed**: Shows event type and summary message
- **Expanded**: Color-formatted JSON with syntax highlighting
  - Keys: Cyan (#00FFFF)
  - Strings: Green (#00FF00)
  - Numbers: Orange (#FFA500)
  - Booleans: Purple (#FF00FF)
  - null: Red (#FF0000)
- **Toggle**: Click log entry to expand/collapse
- **Indicators**: ▶ (collapsed) / ▼ (expanded)

#### Advanced Filtering

**Filter Controls**: Located above log area

**Direction Filters** (left group):
- ☑ Send — Show outgoing messages
- ☑ Recv — Show incoming messages
- ☑ Info — Show internal messages

**Event Type Filters** (right group):
- ☑ Game Events — Lifecycle events (start/end/win/lose)
- ☑ Nukes — Nuclear launch events
- ☑ Alerts — Incoming threat alerts
- ☑ Troops — Troop count updates
- ☑ Sounds — Sound play events
- ☑ System — Hardware diagnostic/test events
- ☑ Heartbeat — Periodic connection pings (15s)

**Filter Behavior**:
- Real-time filtering (instant UI update)
- Multiple filters combinable
- State persisted to `ots-log-filters` (GM storage)
- All filters enabled by default

### 5. Hardware Tab Features

#### Diagnostic Display
- **Request Button**: "🔍 Request Diagnostics"
  - Sends `hardware-diagnostic` command
  - Disabled during request (2s cooldown)
- **Status Grid**: Shows module information
  - Module name and type
  - Initialization status (✓/✗)
  - Pin configurations
  - I2C addresses
  - Error states
- **Empty State**: Prompt to request diagnostics
- **Auto-update**: Refreshes on HARDWARE_DIAGNOSTIC event

### 6. Sound Tab Features

#### Sound Toggles
**Available Sounds**:
- `game_start` — Game Start
- `game_player_death` — Player Death
- `game_victory` — Victory
- `game_defeat` — Defeat

**Toggle Row Format**:
```
☑ Game Start [game_start]  [▶ Test]
```

**Behavior**:
- Checked: Send SOUND_PLAY events on game events
- Unchecked: Suppress sound events
- State persisted to `ots-sound-toggles` (GM storage)

#### Remote Testing
**▶ Test Buttons**:
- One per sound toggle row
- Blue hover effect
- Sends SOUND_PLAY event with `test: true` flag
- Respects toggle state
- No local playback (server/firmware plays sound)

### 7. Settings Panel

#### Access
- Click gear button (⚙) in header
- Floating panel above main HUD
- Independent draggable window

#### WebSocket URL Configuration
- **Input**: Current WebSocket server URL
- **Default**: `ws://localhost:3000/ws`
- **Reset Button**: Restore default (doesn't save)
- **Save Button**: Persist and reconnect
- **Close Button**: Discard changes

### 8. Visual Design

#### Color Scheme
- **Background**: `rgba(15, 23, 42, 0.96)` - Semi-transparent dark
- **Border**: `rgba(148, 163, 184, 0.5)` - Light slate
- **Text**: `#e5e7eb` - Light gray
- **Accent**: Blue for active elements

#### Typography
- **Headers**: 14px bold
- **Body**: 12px monospace (Consolas, Monaco)
- **Logs**: 11px monospace
- **JSON**: 11px monospace with indentation

#### Spacing
- **Padding**: 8-12px on containers
- **Margins**: 4-8px between elements
- **Tab padding**: 8px horizontal, 6px vertical
- **Filter gaps**: 8px between checkboxes

### 9. Persistent State

Storage keys managed via `GM_setValue`/`GM_getValue`:

| Key | Type | Purpose |
|-----|------|---------|
| `ots-hud-pos` | `{left, top}` | Window position |
| `ots-hud-size` | `{width, height}` | Window dimensions |
| `ots-hud-collapsed` | `boolean` | Visibility state |
| `ots-ws-url` | `string` | WebSocket server URL |
| `ots-log-filters` | `LogFilters` | Filter checkbox states |
| `ots-sound-toggles` | `Record<string, boolean>` | Sound toggle states |

### 10. Layout Structure

```
┌──────────────────────────────────────────────┐
│ Header (Status Pills + Heartbeat + Controls)│
│ [WS ●] [GAME ●] [💓]        [⚙] [×]        │
├──────────────────────────────────────────────┤
│ Tab Navigation                               │
│ [Logs] [Hardware] [Sound]                    │
├──────────────────────────────────────────────┤
│                                              │
│ Tab Content Area (Resizable)                │
│                                              │
│  • Logs: Filters + Message Stream           │
│  • Hardware: Diagnostics + Status Grid      │
│  • Sound: Toggles + Test Buttons            │
│                                              │
│                                          ⋰⋰  │ ← Resize Handle
└──────────────────────────────────────────────┘
```

## Implementation Details

### Component Classes

**Main Classes**:
- `Hud` — Main HUD controller (sidebar-hud.ts)
- `BaseWindow` — Draggable window base (window.ts)
- `FloatingPanel` — Settings panel container (window.ts)

**Integration Points**:
- `WsClient` — WebSocket connection and messaging
- `GameBridge` — Game event detection and integration
- `main.user.ts` — Entry point and initialization

### Key Methods

**Hud Class**:
- `pushLog(direction, message, eventType?)` — Add log entry
- `setStatus(status)` — Update WS pill
- `setGameStatus(status)` — Update GAME pill
- `isSoundEnabled(soundId)` — Check sound toggle state
- `addLog(tag, message, eventType?)` — Internal log renderer
- `formatJson(obj, indent)` — Recursive JSON formatter
- `shouldShowLogEntry(entry)` — Filter evaluation

**Settings**:
- `initializeSettingsPanel()` — Create settings UI
- `saveWsUrl()` — Persist and reconnect
- `resetWsUrl()` — Restore default

**Sound**:
- `initializeSoundToggles()` — Create sound UI with test buttons
- `testSound(soundId)` — Send test event
- `onSoundTest` callback — WebSocket event sender

**Hardware**:
- `requestDiagnostics()` — Send diagnostic command
- `updateHardwareDisplay()` — Render diagnostic data
- `parseDiagnosticData(data)` — Extract module info

### Event Handling

**Mouse Events**:
- **Drag**: Header bar mousedown + mousemove
- **Resize**: Handle mousedown + mousemove
- **Tab Switch**: Tab header click
- **Collapse JSON**: Log entry click
- **Toggle Filter**: Checkbox change
- **Toggle Sound**: Checkbox change
- **Test Sound**: Button click

**WebSocket Events**:
- **Message Received**: Parse and add to log
- **Connection State**: Update status pills
- **Heartbeat**: Trigger blink animation
- **Diagnostic Response**: Update hardware display

### Performance Optimizations

1. **Log Limit**: Max 100 entries (FIFO removal)
2. **Filter Caching**: Pre-compute filter states
3. **JSON Lazy Format**: Only format when expanded
4. **Direct DOM**: No virtual DOM overhead
5. **Event Delegation**: Single listener for multiple elements
### CSS Animations

#### Heartbeat Blink
```css
@keyframes heartbeat-blink {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.3; }
}
/* Duration: 300ms */
```

#### Tab Switching
- Instant content swap (no animation)
- Active tab background: Blue
- Smooth visual feedback on hover

## Browser Compatibility

- **Tested**: Chrome/Chromium-based browsers with Tampermonkey
- **Requirements**: 
  - Modern CSS (flexbox, transforms, animations)
  - ES6+ JavaScript (classes, arrow functions, template literals)
  - Tampermonkey GM storage APIs (`GM_getValue`, `GM_setValue`)
  - WebSocket API support

## Future Enhancements

### Planned Features
- Volume control for sound tests
- "Test All" button for sounds
- Filter presets (save/load)
- Export logs to file
- Search in log history
- Custom sound IDs
- Hardware module control
- Keyboard shortcuts
- Themes (dark/light)

### Technical Improvements
- Virtual scrolling for large logs
- Web Workers for JSON formatting
- IndexedDB for log persistence
- Debounced filter updates
- Optimized event detection (Map/Set)

## References

- Main context: `/ots-userscript/copilot-project-context.md`
- Features doc: `/ots-userscript/docs/HUD_FEATURES.md`
- Protocol spec: `/prompts/WEBSOCKET_MESSAGE_SPEC.md`
- Source code: `/ots-userscript/src/hud/sidebar-hud.ts`

---

**Last Updated**: December 29, 2025
**Version**: 2025-12-20.1
**Status**: Fully Implemented
