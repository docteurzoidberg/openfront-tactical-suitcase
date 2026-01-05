# OTS Prompt & Documentation Review Plan

**Created:** January 5, 2026  
**Purpose:** Systematic review of all prompt and documentation files for clarity, accuracy, code synchronization, and redundancy  
**Progress:** 7/72 files reviewed (9.7%) - 1 file relocated to doc/, 2 files created from split, 1 file duplicated

## Review Workflow

Use with `PROMPT_REVIEW_GUIDE.md` to review files one at a time. For each file:

1. Read current content
2. Check code synchronization
3. Identify missing/unclear details
4. Check for redundancy/merge opportunities
5. Update or mark as ✅ reviewed

---

## Root Level Documentation

### Project-Wide
- [x] `.github/copilot-instructions.md` - Workspace-level AI context (451 lines)
  ✅ Reviewed 2026-01-05: Fixed WebSocket paths (ws.ts), removed duplicate repo structure, added -e to pio commands, removed line count reference, removed certs/ folder reference
- [x] `README.md` - Repository root documentation
  ✅ Reviewed 2026-01-05: Fixed GitHub URL to correct repo, doc/ links kept as-is (outside review scope)
- [x] `prompts/WEBSOCKET_MESSAGE_SPEC.md` - WebSocket message specification (SOURCE OF TRUTH)
  ✅ Reviewed 2026-01-05 (DEEP REVIEW): Fixed WebSocket endpoint refs (/ws-script, /ws-ui → /ws). Kept C++ refs per user. 1435 lines reviewed - nuke tracking, sound protocol, hardware modules, timing constants, server behavior all verified accurate. Renamed from protocol-context.md for clarity.
  ✅ Enhanced 2026-01-05 (AI GUIDELINES): Added 🤖 AI Assistant Guidelines sections throughout (1435→2117 lines, 682 lines added). 13 guideline sections cover: Protocol compliance, command implementation, send-nuke, set-troops-percent, connection status, game lifecycle, nuke launches (ATOM/HYDRO/MIRV), alert events (nuclear + invasions), sound playback, hardware diagnostics, nuke outcomes, protocol evolution workflow. Dual-purpose: human-readable reference + AI-specific directives with ✅/❌ code examples, common mistakes, debugging steps.
  ✅ Refactored 2026-01-05 (FILE SPLIT): Split enhanced file into focused documents:
    - `prompts/WEBSOCKET_MESSAGE_SPEC_SOURCE.md` - Temporary source with all AI guidelines (archived)
    - `prompts/WEBSOCKET_MESSAGE_SPEC.md` - Clean protocol specification for AI reference (620 lines) - Single source of truth, concise message format reference, event/command catalog, timing constants
    - `doc/developer/websocket-protocol.md` - Developer implementation guide (430 lines) - Implementation patterns, code examples (TypeScript/C), debugging techniques, testing strategies, best practices
  Updated references in:
    - `.github/copilot-instructions.md` - Added both spec and guide links
    - `ots-userscript/copilot-project-context.md` - Protocol references section
    - `ots-simulator/copilot-project-context.md` - Protocol references section
    - `ots-fw-main/copilot-project-context.md` - Protocol references section
    - `doc/STRUCTURE.md` - Added websocket-protocol.md entry
    - `doc/developer/README.md` - Added WebSocket Protocol section
- [x] ~~`prompts/DEVELOPMENT_SETUP.md`~~ → **RELOCATED to `doc/developer/development-environment.md`**
  ✅ Reviewed & Restructured 2026-01-05: User-facing developer doc, not AI prompt. Merged with existing getting-started.md strategy. Fixed GitHub URL, updated project structure, added -e flags. Content now split: getting-started.md (quick) + development-environment.md (detailed). Removed from prompt review scope (doc/ folder excluded).
- [x] `prompts/GIT_WORKFLOW.md` - Git workflow and commit conventions (AI-oriented)
  ✅ Reviewed & Converted 2026-01-05: Converted from user tutorial to AI prompt (528→228 lines). Now instructs Copilot on: generating commit messages, enforcing protocol change order, handling git operations. Removed PlatformIO section (moved to PLATFORMIO_WORKFLOW.md). Removed user-facing tutorials, collaboration guides, troubleshooting.
- [x] `prompts/PLATFORMIO_WORKFLOW.md` - PlatformIO build system guidance
  ✅ Created 2026-01-05: New AI prompt consolidating PlatformIO knowledge. Covers: -e flag requirement, test environments, build validation, esptool access via pio pkg exec. Explicitly states PlatformIO-only (no idf.py commands). Quick reference for all 3 firmware projects.
- [x] `prompts/RELEASE.md` - Release process AI guidance
  ✅ Reviewed & Converted 2026-01-05: User doc copied to doc/developer/releases.md, prompt rewritten as AI guidance (337→151 lines). AI prompt teaches: when to help (user must ask), suggesting appropriate commands, troubleshooting patterns. Key rule: Do NOT proactively suggest releases. Also created doc/developer/releases.md (337 lines) as complete user reference.

---

## ots-fw-main (ESP32-S3 Firmware)

### Project Context
- [ ] `ots-fw-main/copilot-project-context.md` - Main firmware AI context
- [ ] `ots-fw-main/README.md` - Project overview
- [ ] `ots-fw-main/CHANGELOG.md` - Version history
- [ ] `ots-fw-main/TODO.md` - Active development tasks

### Hardware Module Prompts (prompts/)
- [ ] `ots-fw-main/prompts/PROMPTS_INDEX.md` - Navigation index
- [ ] `ots-fw-main/prompts/ALERT_MODULE_PROMPT.md` - Alert module specification
- [ ] `ots-fw-main/prompts/MAIN_POWER_MODULE_PROMPT.md` - Main power module
- [ ] `ots-fw-main/prompts/NUKE_MODULE_PROMPT.md` - Nuke control module
- [ ] `ots-fw-main/prompts/TROOPS_MODULE_PROMPT.md` - Troops display module
- [ ] `ots-fw-main/prompts/SOUND_MODULE_PROMPT.md` - Audio module integration
- [ ] `ots-fw-main/prompts/SYSTEM_STATUS_MODULE_PROMPT.md` - System status LCD

### Architecture & Development (prompts/)
- [ ] `ots-fw-main/prompts/FIRMWARE_MAIN_PROMPT.md` - Main firmware structure
- [ ] `ots-fw-main/prompts/ARCHITECTURE_PROMPT.md` - System architecture
- [ ] `ots-fw-main/prompts/MODULE_DEVELOPMENT_PROMPT.md` - Module development guide
- [ ] `ots-fw-main/prompts/I2C_COMPONENTS_PROMPT.md` - I2C hardware abstraction
- [ ] `ots-fw-main/prompts/PROTOCOL_CHANGE_PROMPT.md` - Protocol update workflow
- [ ] `ots-fw-main/prompts/WEBSOCKET_PROTOCOL_PROMPT.md` - WebSocket implementation

### Network & Updates (prompts/)
- [ ] `ots-fw-main/prompts/NETWORK_PROVISIONING_PROMPT.md` - WiFi setup
- [ ] `ots-fw-main/prompts/OTA_UPDATE_PROMPT.md` - Firmware OTA updates

### Testing & Debugging (prompts/)
- [ ] `ots-fw-main/prompts/BUILD_AND_TEST_PROMPT.md` - Build system
- [ ] `ots-fw-main/prompts/AUTOMATED_TESTS_PROMPT.md` - Test framework
- [ ] `ots-fw-main/prompts/DEBUGGING_AND_RECOVERY_PROMPT.md` - Debug guide
- [ ] `ots-fw-main/prompts/DOCUMENTATION_AUDIT.md` - Docs audit checklist

### Component Documentation (components/*/COMPONENT_PROMPT.md)
- [ ] `ots-fw-main/components/ads1015_driver/COMPONENT_PROMPT.md` - ADC driver
- [ ] `ots-fw-main/components/esp_http_server_core/COMPONENT_PROMPT.md` - HTTP server
- [ ] `ots-fw-main/components/hd44780_pcf8574/COMPONENT_PROMPT.md` - LCD driver
- [ ] `ots-fw-main/components/mcp23017_driver/COMPONENT_PROMPT.md` - I/O expander
- [ ] `ots-fw-main/components/ws2812_rmt/COMPONENT_PROMPT.md` - RGB LED driver

### Implementation Docs (docs/)
- [ ] `ots-fw-main/docs/COMPONENTS_ARCHITECTURE.md` - Component system design
- [ ] 🗑️ `ots-fw-main/docs/CAN_COMPONENT_IMPLEMENTATION.md` - CAN integration status **[TO BE DELETED - consolidated into CANBUS_MESSAGE_SPEC.md]**
- [ ] 🗑️ `ots-fw-main/docs/CAN_MULTI_MODULE_ROADMAP.md` - Multi-module CAN plan **[TO BE DELETED - consolidated into canbus-protocol.md]**
- [ ] `ots-fw-main/docs/SOUND_MODULE_IMPLEMENTATION.md` - Audio module notes
- [ ] `ots-fw-main/docs/HARDWARE_DIAGNOSTIC_IMPLEMENTATION.md` - Diagnostics system
- [ ] `ots-fw-main/docs/HARDWARE_TEST_INTEGRATION.md` - Test integration
- [ ] `ots-fw-main/docs/HARDWARE_TEST_PLAN.md` - Hardware test strategy
- [ ] `ots-fw-main/docs/TESTING.md` - Test environments guide
- [ ] `ots-fw-main/docs/NAMING_CONVENTIONS.md` - Code style guide
- [ ] `ots-fw-main/docs/ERROR_RECOVERY.md` - Error handling patterns
- [ ] `ots-fw-main/docs/RGB_LED_STATUS.md` - Status LED patterns

### Network & Web (docs/)
- [ ] `ots-fw-main/docs/WIFI_PROVISIONING.md` - WiFi setup implementation
- [ ] `ots-fw-main/docs/WIFI_WEBAPP.md` - Captive portal webapp
- [ ] `ots-fw-main/docs/CERTIFICATE_INSTALLATION_CHROME.md` - SSL cert guide
- [ ] `ots-fw-main/docs/WSS_TEST_GUIDE.md` - WebSocket SSL testing
- [ ] `ots-fw-main/docs/OTA_GUIDE.md` - OTA update procedures
- [ ] `ots-fw-main/docs/OTS_DEVICE_TOOL.md` - CLI management tool

---

## ots-fw-audiomodule (ESP32-A1S Audio)

### Project Context
- [ ] `ots-fw-audiomodule/PROJECT_PROMPT.md` - Audio module AI context
- [ ] `ots-fw-audiomodule/README.md` - Project overview
- [ ] `ots-fw-audiomodule/prompts/BOOT_CAPTURE_PROMPT.md` - Boot analysis tool

### Implementation Docs
- [ ] `ots-fw-audiomodule/docs/BOOT_CAPTURE_TOOL.md` - Boot logging tool
- [ ] `ots-fw-audiomodule/docs/PSRAM_OPTIMIZATION.md` - Memory optimization
- [ ] `ots-fw-audiomodule/docs/SDCARD_AUDIO_SETUP.md` - SD card audio files

---

## ots-fw-shared (Shared Components)

### Project Context
- [ ] `ots-fw-shared/README.md` - Shared components overview

### Component Documentation
- [ ] `ots-fw-shared/components/can_driver/COMPONENT_PROMPT.md` - CAN driver
- [ ] `ots-fw-shared/components/can_driver/README.md` - CAN driver readme
- [ ] `ots-fw-shared/components/can_discovery/COMPONENT_PROMPT.md` - Module discovery
- [ ] `ots-fw-shared/components/can_audiomodule/COMPONENT_PROMPT.md` - Audio protocol

### CAN Protocol Documentation
- [ ] 🗑️ `ots-fw-shared/prompts/CAN_PROTOCOL_ARCHITECTURE.md` - Multi-module architecture **[TO BE DELETED - consolidated into CANBUS_MESSAGE_SPEC.md]**
- [ ] 🗑️ `ots-fw-shared/prompts/CAN_PROTOCOL_INTEGRATION.md` - Integration guide **[TO BE DELETED - consolidated into canbus-protocol.md]**
- [ ] 🗑️ `ots-fw-shared/prompts/CAN_SOUND_PROTOCOL.md` - Sound protocol spec **[TO BE DELETED - consolidated into CANBUS_MESSAGE_SPEC.md]**

---

## ots-fw-cantest (CAN Test Tool)

### Project Context
- [ ] `ots-fw-cantest/README.md` - CAN test tool overview

---

## ots-hardware (Hardware Specs)

### Project Context
- [ ] `ots-hardware/copilot-project-context.md` - Hardware design AI context
- [ ] `ots-hardware/README.md` - Hardware overview

---

## ots-simulator (Nuxt Dashboard)

### Project Context
- [ ] `ots-simulator/copilot-project-context.md` - Server AI context
- [ ] `ots-simulator/README.md` - Project overview
- [ ] `ots-simulator/CHANGELOG.md` - Version history

---

## ots-userscript (Tampermonkey)

### Project Context
- [ ] `ots-userscript/copilot-project-context.md` - Userscript AI context
- [ ] `ots-userscript/CHANGELOG.md` - Version history

---

## ots-shared (TypeScript Types)

### Project Context
- [ ] `ots-shared/README.md` - Shared types overview

---

## ots-website (VitePress Docs)

### Project Context
- [ ] `ots-website/README.md` - Documentation site overview

---

## Review Statistics

**Total Files:** 72  
**Reviewed:** 2  
**Remaining:** 70  
**Progress:** 2.8%

### By Category
- Root Documentation: 2/6 (33.3%)
- ots-fw-main Prompts: 0/21 (0%)
- ots-fw-main Components: 0/5 (0%)
- ots-fw-main Docs: 0/18 (0%)
- ots-fw-audiomodule: 0/5 (0%)
- ots-fw-shared: 0/7 (0%)
- ots-fw-cantest: 0/1 (0%)
- ots-hardware: 0/2 (0%)
- ots-simulator: 0/3 (0%)
- ots-userscript: 0/2 (0%)
- ots-shared: 0/1 (0%)
- ots-website: 0/1 (0%)

---

## Notes

- Excluded `doc/` root folder (user documentation synced to ots-website)
- Excluded `node_modules/`, `.git/`, build outputs, managed_components READMEs
- Focus on prompt files, project context, and technical documentation
- Each file should be reviewed for: clarity, code sync, completeness, redundancy
