# Keypad Module Integration Plans

This directory contains all planning documents for integrating the keypad module into the OTS system.

## Plan Files

### Core Implementation
1. **[01-keypad-firmware-architecture.md](01-keypad-firmware-architecture.md)** - Keypad firmware architecture
   - Matrix scanner module
   - LED controller module
   - CAN handler module
   - Implementation phases

2. **[02-main-controller-integration.md](02-main-controller-integration.md)** - Main controller firmware updates
   - CAN message handling (key events)
   - WebSocket event forwarding (raw key presses)
   - No key mapping (userscript handles this)

3. **[03-can-protocol-specification.md](03-can-protocol-specification.md)** - CAN bus protocol updates
   - Message format definitions
   - CAN ID allocation (0x430-0x43F)
   - C implementation examples
   - Updates to `/prompts/CANBUS_MESSAGE_SPEC.md`

### Integration Plans
4. **[04-websocket-protocol-integration.md](04-websocket-protocol-integration.md)** - WebSocket protocol updates
   - New event types for keypad (KEY_PRESSED/RELEASED, CONNECTED/DISCONNECTED)
   - Broadcast-only (no binding configuration)
   - Updates to `/prompts/WEBSOCKET_MESSAGE_SPEC.md`

5. **[05-dashboard-ui-integration.md](05-dashboard-ui-integration.md)** - Dashboard/simulator UI updates
   - Visual keypad component (visualization only)
   - Real-time key press animation
   - Connection status display
   - Vue component implementation

6. **[06-documentation-plan.md](06-documentation-plan.md)** - Documentation updates
   - User guides (userscript configuration focus)
   - Developer guides (architecture, API)
   - VitePress site updates

7. **[07-userscript-integration.md](07-userscript-integration.md)** - Userscript key mapping
   - localStorage schema for key bindings
   - Key event handling and game action triggering
   - Configuration UI (Tampermonkey menu → new tab)
   - Export/import functionality

## Implementation Order

**✅ Stage 1: Protocol Foundation (COMPLETE)**
- ✅ CAN protocol specification (CANBUS_MESSAGE_SPEC.md v1.2)
- ✅ CAN protocol shared component (can_protocol_keypad, 909 lines)
- ✅ WebSocket protocol specification (WEBSOCKET_MESSAGE_SPEC.md)
- ✅ TypeScript types (ots-shared/src/game.ts)
- ✅ Firmware protocol types (protocol.h/c)

**✅ Stage 2: Firmware Implementation (COMPLETE)** (Week 1-2)
- ✅ Implemented keypad firmware (Plan #1)
  - ✅ Matrix scanner (270 lines): 200Hz GPIO scanning, 20ms debounce, FreeRTOS task
  - ✅ LED controller (200 lines): RMT-based SK6812-MINI-E control, brightness management
  - ✅ CAN handler (200 lines): can_bus_manager integration, MODULE_ANNOUNCE, LED commands
- ✅ Code quality improvements
  - ✅ Removed duplicate CAN protocol definitions
  - ✅ Consolidated ws2812_rmt to ots-fw-shared (now used by both fw-main and fw-keypad)
  - ✅ Optimized ws2812_rmt with change detection (skip transmit when no changes)
- ✅ Verification:
  - ✅ Keypad firmware compiles (15.3KB RAM, 253KB Flash)
  - ✅ Main controller firmware compiles (41.7KB RAM, 1.07MB Flash)

**🔨 Stage 3: Controller Integration** (Week 3) - **IN PROGRESS**
- [x] Implement main controller CAN handlers (Plan #2)
- [x] Implement WebSocket event forwarding (Plan #4)
- [ ] Test: Physical key → CAN → WebSocket → Dashboard

**Stage 4: UI & Userscript** (Week 4)
- Implement dashboard visualization (Plan #5)
- Implement userscript key mapping (Plan #7)
- Create configuration UI in userscript (Plan #7)

**Stage 5: Documentation & Testing** (Week 5)
- Write user guides (Plan #6)
- Update developer documentation (Plan #6)
- End-to-end testing
- Release preparation

## Architecture Summary

**Key Decision**: Key mapping is handled entirely by the **userscript**, not firmware or dashboard.

```
Physical Key → Firmware → CAN → Main Controller → WebSocket → Userscript
                                                                  ↓
                                                        localStorage lookup
                                                                  ↓
                                                         Trigger Game Action
```

**Responsibilities:**
- **Keypad Firmware**: Matrix scanning, key events, CAN transmission
- **Main Controller**: CAN → WebSocket forwarding only
- **Dashboard**: Visualization only (no configuration)
- **Userscript**: ALL key mapping logic + configuration UI

## Default Key Bindings

The userscript provides these default bindings (customizable via config UI):

**Row 1 - Building Actions:**
- K1: Build City (1) | K2: Build Factory (2) | K3: Build Port (3) | K4: Build Defense Post (4)
- K5: Build Missile Launcher (5) | K6: Build SAM (6) | K7: Build Warship (7)

**Row 2 - Game Controls:**
- K8: Zoom In (E) | K9: Zoom Out (Q) | K10: Decrease Attack Ratio (T) | K11: Switch Missile Direction (U)
- K12: Increase Attack Ratio (Y) | K13: Boat Attack (B) | K14: Land Attack (G)

**Row 3 - View Control:**
- K15: Toggle View (Space)

**Note**: Nuke launches (8/9/0) are NOT mapped - use dedicated Nuke Module hardware buttons.

## Quick Links

- **Hardware Specs**: `../ots-hardware/modules/keypad-module.md`
- **PCB Documentation**: `../ots-hardware/pcbs/keypad.md`
- **Firmware Config**: `../include/config.h`
- **Project Overview**: `../PROJECT_PROMPT.md`

## Status Tracking

- [x] Planning complete
- [x] **Stage 1 (Protocol Foundation) complete**
  - [x] CAN protocol component created (can_protocol_keypad in ots-fw-shared)
  - [x] CANBUS_MESSAGE_SPEC.md updated with keypad protocol v1.2
  - [x] WEBSOCKET_MESSAGE_SPEC.md updated with keypad events
  - [x] TypeScript types updated (ots-shared)
  - [x] Firmware protocol updated (protocol.h/c)
- [x] **Stage 2 (Keypad Firmware) complete**
  - [x] Matrix scanner module (matrix_scanner.c/h)
  - [x] LED controller module (led_controller.c/h)
  - [x] CAN handler module (can_handler.c/h)
  - [x] Main integration (main.c)
  - [x] Consolidated ws2812_rmt to shared components
  - [x] Optimized LED driver with change detection
  - [x] Both firmwares verified building
- [ ] **Stage 3 (Main Controller Integration)** - **IN PROGRESS**
   - [x] CAN handlers for keypad events
   - [x] WebSocket event forwarding
  - [ ] End-to-end testing (key → CAN → WebSocket → Dashboard)
- [ ] Dashboard UI implemented (Stage 4)
- [ ] Userscript integration (Stage 4)
- [ ] Documentation written (Stage 5)
- [ ] End-to-end testing (Stage 5)
- [ ] Release ready
