# OTS Documentation Tree Structure

Complete folder and file structure for the `/doc` directory.

## Implementation Status

**Legend:**
- ✅ Implemented
- 🚧 In Progress
- ⏳ Planned

**Last Updated:** January 5, 2026

```
doc/
├── README.md                              # Main documentation index
│
├── user/                                  # 👤 USER DOCUMENTATION
│   ├── README.md                          # User docs table of contents
│   │
│   ├── quick-start.md                     # ⭐ First-time setup guide
│   ├── wifi-setup.md                      # WiFi/network configuration
│   ├── userscript-install.md              # Browser extension setup
│   ├── userscript-interface.md            # HUD tabs and features
│   ├── device-guide.md                    # Physical hardware overview
│   ├── troubleshooting.md                 # Common issues and fixes
│   ├── FAQ.md                             # Frequently asked questions
│   │
│   ├── modules/                           # Hardware module guides
│   │   ├── README.md                      # Modules overview
│   │   ├── nuke-module.md                 # Nuke Control Panel guide
│   │   ├── alert-module.md                # Alert indicators guide
│   │   ├── troops-module.md               # Troops counter/slider guide
│   │   └── main-power.md                  # Connection status LED
│   │
│   └── advanced/                          # Advanced user topics
│       ├── firmware-updates.md            # OTA update procedure
│       ├── custom-sounds.md               # Adding custom sound files
│       └── network-config.md              # Advanced WiFi settings
│
├── developer/                             # 🛠️ DEVELOPER DOCUMENTATION
│   ├── README.md                          # Developer docs table of contents
│   │
│   ├── getting-started.md                 # ⭐ Dev environment setup
│   ├── development-environment.md         # Comprehensive IDE setup and workflows
│   ├── repository-overview.md             # Codebase structure
│   ├── websocket-protocol.md              # WebSocket protocol implementation guide
│   ├── canbus-protocol.md                 # CAN bus protocol implementation guide
│   ├── releases.md                        # Release process and version management
│   ├── firmware-development.md            # ESP32 firmware dev guide
│   ├── server-development.md              # Nuxt dashboard dev guide
│   ├── userscript-development.md          # Browser extension dev guide
│   ├── CONTRIBUTING.md                    # Contribution guidelines
│   ├── FAQ.md                             # Developer FAQ
│   │
│   ├── architecture/                      # System design docs
│   │   ├── README.md                      # Architecture overview
│   │   ├── protocol.md                    # WebSocket protocol spec
│   │   ├── firmware.md                    # Firmware architecture
│   │   ├── event-system.md                # Event dispatcher design
│   │   ├── module-system.md               # Hardware module interface
│   │   └── shared-components.md           # Reusable code structure
│   │
│   ├── hardware/                          # Hardware design docs
│   │   ├── README.md                      # Hardware overview
│   │   ├── main-controller.md             # ESP32-S3 controller
│   │   ├── modules.md                     # Module specifications
│   │   ├── i2c-architecture.md            # I2C bus design
│   │   ├── can-bus.md                     # CAN bus topology
│   │   ├── pcb-designs.md                 # Circuit boards
│   │   └── assembly.md                    # Assembly instructions
│   │
│   ├── workflows/                         # Common development tasks
│   │   ├── add-hardware-module.md         # Create new module
│   │   ├── add-game-event.md              # Add protocol event
│   │   ├── protocol-changes.md            # Modify protocol
│   │   ├── testing-hardware.md            # Hardware test procedure
│   │   └── ota-updates.md                 # Firmware update process
│   │
│   ├── standards/                         # Code and process standards
│   │   ├── coding-standards.md            # Style guides
│   │   ├── naming-conventions.md          # File/variable naming
│   │   ├── git-workflow.md                # Git branching/commits
│   │   └── documentation.md               # Docs requirements
│   │
│   ├── testing/                           # Testing documentation
│   │   ├── README.md                      # Testing overview
│   │   ├── hardware-test-plan.md          # Hardware testing
│   │   ├── firmware-testing.md            # Firmware test guide
│   │   ├── integration-testing.md         # End-to-end tests
│   │   └── can-bus-testing.md             # CAN protocol validation
│   │
│   ├── deployment/                        # Release and deployment
│   │   ├── release-process.md             # Version releases
│   │   ├── firmware-updates.md            # OTA deployment
│   │   └── build-systems.md               # CI/CD setup
│   │
│   └── reference/                         # API and component docs
│       ├── api/                           # API documentation
│       │   ├── firmware-api.md            # C API reference
│       │   ├── server-api.md              # TypeScript API
│       │   └── userscript-api.md          # Userscript API
│       │
│       └── components/                    # Hardware driver docs
│           ├── mcp23017.md                # I/O expander
│           ├── ads1015.md                 # ADC driver
│           ├── hd44780.md                 # LCD driver
│           ├── ws2812.md                  # RGB LED driver
│           └── can-driver.md              # CAN/TWAI driver
│
└── assets/                                # Documentation assets
    ├── images/                            # Screenshots, diagrams
    │   ├── device-photos/                 # Hardware photos
    │   ├── userscript-ui/                 # HUD screenshots
    │   ├── architecture/                  # System diagrams
    │   └── wiring/                        # Wiring diagrams
    │
    └── videos/                            # Video tutorials (future)
        ├── setup-guide.mp4
        └── troubleshooting.mp4
```

## Documentation Principles

### User Documentation (`user/`)
- **Audience**: Device owners, gamers, non-technical users
- **Tone**: Friendly, clear, step-by-step instructions
- **Content**: 
  - Getting started guides
  - How-to tutorials
  - Troubleshooting
  - Feature explanations
- **No Assumption**: Assume no technical knowledge
- **Visual**: Heavy use of screenshots and photos

### Developer Documentation (`developer/`)
- **Audience**: Developers, makers, contributors
- **Tone**: Technical, precise, reference-oriented
- **Content**:
  - Architecture explanations
  - API documentation
  - Development workflows
  - Code standards
- **Assumption**: Intermediate programming knowledge
- **Visual**: Diagrams, code snippets, architecture charts

## File Naming Conventions

- Use **kebab-case** for all markdown files: `wifi-setup.md`
- Use **descriptive names**: `add-hardware-module.md` not `new-module.md`
- Keep names **short but clear**: `firmware-development.md` not `how-to-develop-firmware.md`
- Use **consistent naming**: `*-guide.md` for guides, `*-reference.md` for references

## Content Guidelines

### All Documentation
- Start with **clear title** and **brief description**
- Include **table of contents** for long documents
- Use **code blocks** with language tags
- Include **links** to related documentation
- Add **last updated** date at bottom

### User Documentation
- Start with **prerequisites** section
- Use **numbered steps** for procedures
- Include **screenshots** where helpful
- Add **troubleshooting** section at end
- Provide **next steps** or related guides

### Developer Documentation
- Start with **overview** and **goals**
- Include **code examples** for concepts
- Link to **related code** in repository
- Document **edge cases** and **limitations**
- Include **testing** section

## Documentation Workflow

1. **Plan**: Outline structure in this file first
2. **Draft**: Create markdown files with basic structure
3. **Review**: Check for accuracy and clarity
4. **Visual**: Add diagrams, screenshots, code examples
5. **Cross-link**: Ensure proper navigation between docs
6. **Test**: Follow guides to verify accuracy
7. **Update**: Keep docs in sync with code changes

## Migration Notes

**Existing Documentation to Move/Consolidate:**

From `ots-fw-main/docs/`:
- `OTA_GUIDE.md` → `user/advanced/firmware-updates.md`
- `TESTING.md` → `developer/testing/firmware-testing.md`
- `COMPONENTS_ARCHITECTURE.md` → `developer/architecture/firmware.md`
- `NAMING_CONVENTIONS.md` → `developer/standards/naming-conventions.md`

From `ots-fw-main/prompts/`:
- Module prompts → `developer/workflows/add-hardware-module.md`
- Protocol prompts → `developer/workflows/protocol-changes.md`

From `ots-userscript/docs/`:
- HUD docs → `user/userscript-interface.md`
- Architecture → `developer/architecture/userscript.md`

From `prompts/`:
- `WEBSOCKET_MESSAGE_SPEC.md` → Link as authoritative source
- `developer/development-environment.md` - Comprehensive environment setup
- `GIT_WORKFLOW.md` → `developer/standards/git-workflow.md`

## Next Steps

1. Create user quick-start guide (highest priority)
2. Create developer getting-started guide
3. Consolidate existing docs into new structure
4. Add screenshots and diagrams
5. Create video tutorials (future)
