# OTS Firmware Documentation Audit

**Date**: 2025-01-XX  
**Purpose**: Organize prompts and docs for better AI agent navigation

## Summary

✅ **Prompts**: 12 files (well-organized, task-oriented)  
📚 **Docs**: 21 files (mix of guides, notes, and deprecated content)  
🆕 **Created**: ARCHITECTURE_PROMPT.md (comprehensive system overview)  
♻️ **Updated**: PROMPTS_INDEX.md (better structure and navigation)

## Prompt Files (prompts/)

### Core System Prompts ✅
- `ARCHITECTURE_PROMPT.md` - **NEW** Component structure, initialization order, design patterns
- `FIRMWARE_MAIN_PROMPT.md` - Application entry point and boot sequence
- `PROTOCOL_CHANGE_PROMPT.md` - Syncing protocol changes across firmware/server/userscript
- `BUILD_AND_TEST_PROMPT.md` - PlatformIO commands and test environments
- `DEBUGGING_AND_RECOVERY_PROMPT.md` - Boot loops, error recovery, flash issues
- `MODULE_DEVELOPMENT_PROMPT.md` - Creating new hardware modules
- `AUTOMATED_TESTS_PROMPT.md` - Test automation on real hardware

### Hardware Module Prompts ✅
- `MAIN_POWER_MODULE_PROMPT.md` - LINK LED and connection status
- `NUKE_MODULE_PROMPT.md` - Nuke launch buttons and LEDs
- `ALERT_MODULE_PROMPT.md` - Incoming threat indicators
- `TROOPS_MODULE_PROMPT.md` - LCD display + slider + ADC
- `SOUND_MODULE_PROMPT.md` - Audio feedback and alerts

### Status
✅ All existing prompts are **well-structured** and **task-oriented**  
✅ No duplicate content found  
✅ PROMPTS_INDEX.md updated with clear navigation

## Documentation Files (docs/)

### User Guides (Keep as-is) 📚
- `TESTING.md` - How to run hardware test environments
- `OTA_GUIDE.md` - Over-the-air update procedures
- `OTS_DEVICE_TOOL.md` - CLI tool for device management
- `WSS_TEST_GUIDE.md` - WebSocket testing guide
- `NAMING_CONVENTIONS.md` - Code style and naming rules
- `RGB_LED_STATUS.md` - Status LED behavior reference
- `ERROR_RECOVERY.md` - Boot failure recovery steps

### Implementation Notes (Keep for reference) 📝
- `HARDWARE_DIAGNOSTIC_IMPLEMENTATION.md` - Diagnostic system details
- `HARDWARE_TEST_INTEGRATION.md` - Test integration notes
- `HARDWARE_TEST_PLAN.md` - Testing strategy
- `GAME_END_SCREEN.md` - Game end display implementation
- `LCD_DISPLAY_MODES.md` - LCD screen modes reference
- `REFACTORING_ADC_TASK.md` - ADC task refactoring notes
- `WIFI_PROVISIONING.md` - WiFi setup flow
- `WIFI_WEBAPP.md` - WiFi config web UI
- `PROTOCOL_ALIGNMENT_CHECK.md` - Protocol sync verification
- `OTA_VERIFICATION_REPORT.md` - OTA testing results
- `CERTIFICATE_INSTALLATION_CHROME.md` - TLS certificate guide
- `TEST_WIFI_PROVISIONING_FLOW.md` - WiFi provisioning test

### Deprecated (Marked for review) ⚠️
- `IMPROVEMENTS.md` - Outdated improvement suggestions (most implemented or obsolete)
- `IMPROV_SERIAL.md` - Feature removed (replaced by captive portal)

## Missing Prompts (Identified)

These system components need dedicated prompt files:

### Priority 1 (Core Systems)
- [ ] `SYSTEM_STATUS_MODULE_PROMPT.md`
  - LCD display screens and game state visualization
  - Screen transitions and event-to-screen mappings
  - References: `DISPLAY_SCREENS_SPEC.md`, `LCD_DISPLAY_MODES.md`

- [ ] `WEBSOCKET_PROTOCOL_PROMPT.md`
  - WebSocket message handling and serialization
  - cJSON usage patterns
  - Message routing to modules
  - References: `protocol-context.md`, `ws_server.c`

### Priority 2 (Infrastructure)
- [ ] `I2C_COMPONENTS_PROMPT.md`
  - Creating reusable I2C device drivers
  - Component structure and CMakeLists.txt
  - Best practices for I2C communication
  - References: existing components (hd44780_pcf8574, ads1015_driver)

- [ ] `NETWORK_PROVISIONING_PROMPT.md`
  - WiFi setup and captive portal
  - Credential storage (NVS)
  - Web UI integration
  - References: `WIFI_PROVISIONING.md`, `WIFI_WEBAPP.md`

- [ ] `OTA_UPDATE_PROMPT.md`
  - Over-the-air firmware update implementation
  - Dual partition management
  - Error handling and rollback
  - References: `OTA_GUIDE.md`, `ota_manager.c`

## Recommendations

### Immediate Actions ✅
1. ✅ Updated PROMPTS_INDEX.md with better structure
2. ✅ Created ARCHITECTURE_PROMPT.md for system overview
3. ✅ Documented missing prompts above

### Next Steps
1. Create missing Priority 1 prompts (SYSTEM_STATUS_MODULE, WEBSOCKET_PROTOCOL)
2. Create missing Priority 2 prompts (I2C_COMPONENTS, NETWORK_PROVISIONING, OTA_UPDATE)
3. Review deprecated docs:
   - Remove or archive IMPROVEMENTS.md if truly obsolete
   - Remove IMPROV_SERIAL.md (feature removed)
4. Consider consolidating similar implementation notes:
   - HARDWARE_* docs could potentially merge
   - WiFi docs could potentially merge

### Documentation Principles
- **Prompts (prompts/)**: Task-oriented "how to" guides for AI agents
- **Docs (docs/)**: Reference documentation for humans and context
- **No duplication**: Link between files rather than copying content
- **Single purpose**: Each file should have one clear focus
- **Version control**: Document when content becomes obsolete

## File Statistics

```
prompts/: 12 files (3 created recently: ARCHITECTURE, AUTOMATED_TESTS, updated PROMPTS_INDEX)
docs/: 21 files (2 marked deprecated)
Total: 33 documentation files
```

## Validation Checks

✅ All prompt files follow task-oriented format  
✅ No duplicate content between prompts  
✅ Clear separation between prompts and docs  
✅ PROMPTS_INDEX.md provides good navigation  
✅ copilot-project-context.md references prompt structure  
⚠️ 5 missing prompts identified for key systems  
⚠️ 2 deprecated docs need removal/archival

## References

- Root protocol: `/prompts/protocol-context.md`
- Hardware specs: `/ots-hardware/modules/`
- Server context: `/ots-simulator/copilot-project-context.md`
- Userscript context: `/ots-userscript/copilot-project-context.md`
