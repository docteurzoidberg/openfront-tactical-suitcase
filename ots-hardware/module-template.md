# Module: [Module Name]

Template for defining a hardware module specification.

## Overview
- **Module Name**: [Descriptive name]
- **Size**: [Physical unit size in units, e.g., 8U, 16U]
- **Game State Domain**: [What aspect of game state this handles - inbound state or outbound commands]
- **Hardware Interface**: [e.g., direct I2C via MCP23017, CAN bus, daughter board with custom protocol]

## Description
[Brief description of module purpose and functionality]

## Hardware Components
List physical components:
- [ ] Component 1 (e.g., LED strip, buttons, display)
- [ ] Component 2
- [ ] etc.

### Communication Protocol
[Describe how the main controller communicates with this module's hardware]
- **Read operations**: [What data is read and how]
- **Write operations**: [What data is written and how]

## Game State Mapping

Define the data this module uses:

```typescript
// For modules that consume game state (inbound)
type ModuleState = {
  // Example: score, playerCount, mapName
}

// For modules that send commands (outbound)
type ModuleCommand = {
  action: 'action_name'
  params: {
    // command parameters
  }
}

// For modules that receive events from game
type ModuleEvent = {
  type: 'event_type'
  // event data
}
```

## Module Behavior

### On State Update (if applicable)
1. [How module receives state from main controller]
2. [How module processes/displays the state]

### On Input (if applicable)
1. [How module detects input]
2. [How module sends data to main controller]
3. [Any local feedback behavior]

## UI Component Mockup

Vue component path: `ots-server/app/components/hardware/[ModuleName].vue`

### Props
```typescript
interface ModuleProps {
  // Props needed to render module state
  connected: boolean
}
```

### Emits
```typescript
interface ModuleEmits {
  // Events emitted on user interaction
}
```

### Visual Design
[Description of UI appearance matching physical module]
- [ ] Visual element 1
- [ ] Visual element 2

## Firmware Implementation

### Header
`ots-fw-main/include/modules/[module_name].h`

### Source
`ots-fw-main/src/modules/[module_name].cpp`

Key methods:
- `init()`: [What it initializes]
- [Other key methods and their purposes]

## Notes
[Any additional notes, challenges, or future enhancements]
