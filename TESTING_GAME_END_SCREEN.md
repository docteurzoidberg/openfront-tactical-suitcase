# Game End Screen - Testing Guide

## Quick Test Setup

### 1. Install Updated Userscript

The userscript has been rebuilt with game outcome detection. Install it in Tampermonkey:

**File**: `ots-userscript/build/userscript.ots.user.js`

**Installation**:
1. Open Tampermonkey dashboard
2. Click "Utilities" tab
3. Click "Import from file" or drag-drop the userscript
4. Or: Open the file in browser to auto-install

### 2. Flash Updated Firmware

The firmware has been built with the game end screen implementation.

**Option A: Via PlatformIO**
```bash
cd /home/drzoid/dev/openfront/ots/ots-fw-main
pio run -t upload
```

**Option B: Via OTA**
```bash
curl -X POST --data-binary @.pio/build/esp32-s3-dev/firmware.bin http://ots-fw-main.local:3232/update
```

### 3. Start OTS Server

```bash
cd /home/drzoid/dev/openfront/ots/ots-server
npm run dev
```

## Testing Scenarios

### Scenario 1: Victory Test

**Steps**:
1. Open OpenFront.io in browser with userscript installed
2. Start a game
3. Play until you win (eliminate all opponents)
4. Observe the LCD display

**Expected Results**:
- **Browser Console**: `[GameBridge] Game ended - VICTORY!`
- **Firmware Serial**: `I (xxxxx) system_status: Player won!`
- **LCD Display**:
  ```
  ┌────────────────┐
  │   VICTORY!     │
  │ Good Game!     │
  └────────────────┘
  ```
- **Duration**: 5 seconds, then returns to status screen

### Scenario 2: Defeat Test

**Steps**:
1. Open OpenFront.io in browser
2. Start a game
3. Let yourself get eliminated (lose all territories)
4. Observe the LCD display

**Expected Results**:
- **Browser Console**: `[GameBridge] Game ended - DEFEAT`
- **Firmware Serial**: `I (xxxxx) system_status: Player lost!`
- **LCD Display**:
  ```
  ┌────────────────┐
  │    DEFEAT      │
  │ Good Game!     │
  └────────────────┘
  ```
- **Duration**: 5 seconds, then returns to status screen

### Scenario 3: Quick Game Test

For faster testing, you can simulate quick wins/losses:

**Quick Win** (in game):
- Use console: Nuke all opponent capitals immediately
- Your territories intact = WIN event

**Quick Loss** (in game):
- Abandon your capital, let AI take it
- All your territories lost = LOOSE event

## Debug Monitoring

### Browser Console

Open developer tools (F12) and watch for these messages:

**Game Start**:
```
[GameBridge] Game started
[WebSocket] Sending event: GAME_START
```

**Victory**:
```
[GameAPI] Checking win status...
[GameAPI] Game over: true, Player eliminated: false
[GameBridge] Game ended - VICTORY!
[WebSocket] Sending event: WIN
```

**Defeat**:
```
[GameAPI] Checking win status...
[GameAPI] Game over: true, Player eliminated: true
[GameBridge] Game ended - DEFEAT
[WebSocket] Sending event: LOOSE
```

### Serial Monitor

Connect to firmware serial port to see event processing:

```bash
pio device monitor
```

**Expected Logs**:

**Game Start**:
```
I (12345) ws_client: Event received: GAME_START
I (12345) game_state: Game phase changed: LOBBY -> IN_GAME
```

**Victory**:
```
I (45678) ws_client: Event received: WIN
I (45678) event_dispatcher: Dispatching event: GAME_EVENT_WIN to 4 modules
I (45678) system_status: Player won!
I (45678) system_status: Showing victory screen
I (50678) system_status: Game end screen timeout, returning to status
I (50678) game_state: Game phase changed: WON -> ENDED
```

**Defeat**:
```
I (45678) ws_client: Event received: LOOSE
I (45678) event_dispatcher: Dispatching event: GAME_EVENT_LOOSE to 4 modules
I (45678) system_status: Player lost!
I (45678) system_status: Showing defeat screen
I (50678) system_status: Game end screen timeout, returning to status
I (50678) game_state: Game phase changed: LOST -> ENDED
```

## Troubleshooting

### Issue: No game end screen shown

**Check**:
1. Verify userscript is loaded: Look for `[OTS Userscript] Starting...` in console
2. Check WebSocket connection: LINK LED should be solid ON
3. Verify game actually ended: Game should show victory/defeat screen in browser
4. Check serial monitor for received events

**Solution**:
- Reinstall userscript if not loaded
- Check network connection if WebSocket not connected
- Ensure game reaches actual end state (all opponents eliminated or you eliminated)

### Issue: Shows "GAME_END" instead of WIN/LOOSE

**Cause**: `didPlayerWin()` returned `null` (couldn't determine outcome)

**Check**:
1. Browser console for `[GameAPI] Error checking win status`
2. Game state: Is `myPlayer` available? Is `gameOver()` returning true?

**Solution**:
- This is expected if game crashes or disconnects before proper end
- Normal gameplay should always return win/loss status

### Issue: Screen stays on game end forever

**Check**:
1. Serial monitor for timeout message
2. System time: `esp_timer_get_time()` working?

**Solution**:
- Reset device if timer is stuck
- Check for firmware bugs in timeout logic

## Known Limitations

1. **5-Second Display**: Fixed duration, not configurable via UI yet
2. **No Statistics**: Only shows victory/defeat, no detailed stats
3. **No Animations**: Static text display
4. **Single Line**: Could use scrolling for more information

## Next Steps

After confirming game end screen works:

1. **Add Statistics**:
   - Show score, territories, troops deployed
   - Requires userscript to send game stats in event data

2. **Configurable Timeout**:
   - Add WebSocket command to adjust display duration
   - Store in NVS for persistence

3. **Animations**:
   - Victory: Blinking text
   - Defeat: Dimmed display

4. **Sound Effects**:
   - Connect buzzer module
   - Play victory/defeat tones

---

**Build Info**:
- Userscript: ✅ Built successfully
- Firmware: ✅ Built successfully (7.05s)
- Flash: 49.0% (1,028,584 bytes)
- Ready to test!
