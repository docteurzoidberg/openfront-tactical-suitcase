# OTS Weekly Announcements

Collection of weekly changelog announcements for Discord.

---

## 🎮 Week of December 16-20, 2025

### 🎯 Game End Detection & LCD Screens
**Just now:**
- ✅ **Reliable game end detection** - Now correctly detects wins/losses using dual-method detection:
  - Immediate death detection via `myPlayer.isAlive()`
  - Win update polling via `game.updatesSinceLastTick()` (GameUpdateType.Win)
  - Properly distinguishes team vs individual victories
- 🖥️ **Persistent victory/defeat screens** - LCD displays now stay until you return to lobby (no 5-second timeout)

### 🚀 Major Features This Week
- 🎯 **Ghost structure targeting** - Nuke launches now use the game's native targeting system (fixes wrong-tile bug)
- 📊 **Full troop monitoring** - Real-time troop count and attack ratio tracking
- 💾 **Nuke tracking by unitID** - LEDs stay on until specific nuke resolves (no more timers!)
- 🎨 **LCD Matrix font restored** - Beautiful custom font back in troops display
- 🎛️ **Attack ratio control** - Server can now adjust slider via UIState API

### 🛠️ Technical Improvements
- 📦 Protocol event consolidation (cleaner event system)
- 🎨 Hardware module UI polish (better visuals & animations)
- 🪟 Userscript HUD improvements (better layout, log filtering)
- 🐛 Event list fix (removed 100-event limit)
- 🧹 Code cleanup (removed unused types, better organization)

### 🔧 Architecture
- Phase 4 firmware refactoring complete
- Encapsulated button handling in nuke module
- Shared types consolidated in ots-shared package

**Status:** Dashboard emulator fully functional, firmware modules implemented, ready for hardware testing! 🎉

---
