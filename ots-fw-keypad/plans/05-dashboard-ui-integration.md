# Dashboard UI Integration - Keypad Module

This document outlines the UI components and features needed in the OTS dashboard to support keypad configuration and visualization.

## Current Status (Feb 2026)

- ⏳ Dashboard keypad UI implementation is still pending
- ⏳ No dedicated keypad module component exists yet in `ots-simulator`
- ✅ Firmware/main-controller/userscript event path is implemented
- ✅ Userscript now provides live keypad visualization and action configuration
- ⏳ Remaining: implement Plan #5 components in simulator dashboard

## Overview

The dashboard provides:
1. **Visual keypad component** - Real-time key press visualization
2. **LED state display** - Show current LED colors (future feature)
3. **Connection status** - Show keypad online/offline state

**Note**: Key binding configuration is handled by the userscript (not dashboard).

**Default Userscript Bindings** (for context):
- **Row 1** (K1-K7): Building actions (City, Factory, Port, Defense, Missile, SAM, Warship)
- **Row 2** (K8-K14): Game controls (Zoom, Attack ratio, Missile direction, Attacks)
- **Row 3** (K15): Toggle view (Spacebar)

Dashboard displays keys as "K1" through "K15" - users configure actual game actions in userscript.

## Files to Create/Modify

### New Components (ots-simulator/app/components/hardware/)

```
hardware/
  keypad/
    KeypadModule.vue           - Main keypad module component
    KeypadVisualizer.vue       - 15-key visual display
    KeypadLedIndicator.vue     - Single key LED display (future)
    KeypadKeyButton.vue        - Single key component
```

**Removed**: `KeypadBindingEditor.vue` (userscript handles configuration)

### Modified Components

- `app/pages/index.vue` - Add keypad module to dashboard grid
- `app/composables/useGameSocket.ts` - Handle keypad events

---

## Component Design

### 1. KeypadModule.vue

**Purpose**: Main container for keypad module (4U size)

**Features**:
- Shows keypad visualizer
- Displays connection status
- Real-time key press animation

**Layout**:  
```
┌─────────────────────────────────────────┐
│ Keypad Module            [✓ Connected] │
├─────────────────────────────────────────┤
│                                         │
│  [K1] [K2] [K3] [K4] [K5] [K6] [K7]     │
│  [K8] [K9] [K10][K11][K12][K13][K14]    │
│         [     K15 (Spacebar)     ]      │
│                                         │
└─────────────────────────────────────────┘
```

**Props**: None (uses composable for state)

**State**:
```typescript
interface KeypadModuleState {
  connected: boolean;
  keyStates: Map<number, boolean>;  // keyId → pressed
  ledStates: Map<number, LedState>; // keyId → LED state (future)
}
```

**Note**: Keys labeled K1-K15, no action labels (userscript handles mapping)

---

### 2. KeypadVisualizer.vue

**Purpose**: Display 15 keys with press states

**Features**:
- Visual representation of physical layout
- Shows LED colors in real-time (future)
- Displays key IDs (K1-K15)
- Press animation on key events

**Key Layout**:
```vue
<template>
  <div class="keypad-grid">
    <!-- Row 1 -->
    <KeypadKeyButton
      v-for="keyId in [1,2,3,4,5,6,7]"
      :key="keyId"
      :keyId="keyId"
      :pressed="keyStates.get(keyId)"
      :led="ledStates.get(keyId)"
      :binding="bindings.get(keyId)"
      @click="editMode && onKeyClick(keyId)"
    />
    
    <!-- Row 2 -->
    <KeypadKeyButton
      v-for="keyId in [8,9,10,11,12,13,14]"
      :key="keyId"
      :keyId="keyId"
      :pressed="keyStates.get(keyId)"
      :led="ledStates.get(keyId)"
      :binding="bindings.get(keyId)"
      @click="editMode && onKeyClick(keyId)"
    />
    
    <!-- Row 3 (Spacebar) -->
    <KeypadKeyButton
      :keyId="15"
      :pressed="keyStates.get(15)"
      :led="ledStates.get(15)"
      :binding="bindings.get(15)"
      :wide="true"
      @click="editMode && onKeyClick(15)"
      class="col-span-4 col-start-2"
    />
  </div>
</template>

<style scoped>
.keypad-grid {
  display: grid;
  grid-template-columns: repeat(7, 1fr);
  gap: 0.5rem;
  padding: 1rem;
}
</style>
```

---

### 3. KeypadKeyButton.vue

**Purpose**: Single key visual representation

**Features**:
- Shows key ID (K1-K15)
- LED color indicator (border or background glow, future)
- Press animation

**Visual States**:
- **Idle**: Gray background
- **Pressed**: Scale down animation, brighter background
- **LED On** (future): Colored border/glow matching RGB

**Template**:
```vue
<template>
  <button
    class="key-button"
    :class="{
      'pressed': pressed,
      'led-on': led?.on,
      'wide': wide
    }"
    :style="ledStyle"
  >
    <div class="key-label">K{{ keyId }}</div>
    <div v-if="led?.on" class="led-indicator" :style="ledColorStyle"></div>
  </button>
</template>

<script setup lang="ts">
import type { LedState } from '~/types/keypad'

interface Props {
  keyId: number
  pressed?: boolean
  led?: LedState
  wide?: boolean
}

const props = defineProps<Props>()

const ledStyle = computed(() => {
  if (!props.led?.on) return {}
  
  const { r, g, b } = props.led.color
  return {
    borderColor: `rgb(${r}, ${g}, ${b})`,
    boxShadow: `0 0 10px rgba(${r}, ${g}, ${b}, 0.5)`
  }
})

const ledColorStyle = computed(() => {
  if (!props.led?.on) return {}
  
  const { r, g, b } = props.led.color
  return {
    backgroundColor: `rgb(${r}, ${g}, ${b})`
  }
})
</script>

<style scoped>
.key-button {
  @apply relative flex items-center justify-center;
  @apply bg-gray-700 border-2 border-gray-600 rounded;
  @apply h-16 transition-all duration-100;
}

.key-button.pressed {
  @apply scale-95 bg-gray-500;
}

.key-button.led-on {
  @apply border-4;
}

.key-button.wide {
  @apply col-span-4;
}

.key-label {
  @apply text-sm font-medium text-gray-200;
}

.led-indicator {
  @apply absolute top-1 right-1 w-2 h-2 rounded-full;
}
</style>
```

---

### 4. KeypadLedIndicator.vue (Future)

**Purpose**: Small LED status indicator (for compact views)

**Features**:
- Circular LED dot
- Shows RGB color
- On/off state
- Tooltip with key info

**Template**:
```vue
<template>
  <div
    class="led-dot"
    :class="{ 'on': led.on }"
    :style="ledStyle"
    v-tooltip="tooltipText"
  />
</template>

<script setup lang="ts">
interface Props {
  keyId: number
  led: LedState
  binding?: KeyBinding
}

const props = defineProps<Props>()

const ledStyle = computed(() => {
  if (!props.led.on) return { backgroundColor: '#374151' }
  
  const { r, g, b } = props.led.color
  return {
    backgroundColor: `rgb(${r}, ${g}, ${b})`,
    boxShadow: `0 0 8px rgba(${r}, ${g}, ${b}, 0.8)`
  }
})

const tooltipText = computed(() => 
  `Key ${props.keyId}: ${props.binding?.label || 'Unassigned'}`
)
</script>

<style scoped>
.led-dot {
  @apply w-3 h-3 rounded-full transition-all;
}

.led-dot.on {
  @apply animate-pulse;
}
</style>
```

---

## Composable: useKeypad

**File**: `app/composables/useKeypad.ts`

**Purpose**: Centralized keypad state management (visualization only)

**Note**: Key binding configuration is handled by userscript, not dashboard.

```typescript
export const useKeypad = () => {
  const keyStates = ref<Map<number, boolean>>(new Map())
  const ledStates = ref<Map<number, LedState>>(new Map())
  const connected = ref(false)
  const firmwareVersion = ref('')
  
  const { socket } = useGameSocket()
  
  // Initialize default states
  onMounted(() => {
    for (let i = 1; i <= 15; i++) {
      keyStates.value.set(i, false)
      ledStates.value.set(i, { on: false, color: { r: 0, g: 0, b: 0 } })
    }
  })
  
  // Handle incoming events
  watch(() => socket.value?.lastMessage, (message) => {
    if (!message) return
    
    switch (message.event) {
      case 'KEYPAD_KEY_PRESSED':
        handleKeyPressed(message.data)
        break
      case 'KEYPAD_KEY_RELEASED':
        handleKeyReleased(message.data)
        break
      case 'KEYPAD_CONNECTED':
        handleConnected(message.data)
        break
      case 'KEYPAD_DISCONNECTED':
        handleDisconnected()
        break
    }
  })
  
  function handleKeyPressed(data: KeypadKeyEventData) {
    keyStates.value.set(data.keyId, true)
    
    // Auto-release after animation
    setTimeout(() => {
      keyStates.value.set(data.keyId, false)
    }, 200)
  }
  
  function handleKeyReleased(data: KeypadKeyEventData) {
    keyStates.value.set(data.keyId, false)
  }
  
  function handleConnected(data: any) {
    connected.value = true
    firmwareVersion.value = data.firmwareVersion || 'Unknown'
  }
  
  function handleDisconnected() {
    connected.value = false
    firmwareVersion.value = ''
  }
  
  return {
    keyStates: readonly(keyStates),
    ledStates: readonly(ledStates),
    connected: readonly(connected),
    firmwareVersion: readonly(firmwareVersion)
  }
}
```

---

## TypeScript Types

**File**: `app/types/keypad.ts`

```typescript
export interface LedState {
  on: boolean
  color: {
    r: number
    g: number
    b: number
  }
}
```

**Note**: No `KeyBinding` or `KeypadState` types needed - userscript handles bindings locally.

---

## Integration with Dashboard

### Update app/pages/index.vue

Add keypad module to hardware grid:

```vue
<template>
  <div class="hardware-grid">
    <!-- Existing modules -->
    <MainPowerModule />
    <TroopsModule />
    <NukeModule />
    
    <!-- New keypad module -->
    <KeypadModule />
    
    <AlertModule />
    <SoundModule />
  </div>
</template>
```

---

## Styling Guidelines

### Colors
- **Background**: Gray-700 (`#374151`)
- **Border**: Gray-600 (`#4B5563`)
- **Text**: Gray-200 (`#E5E7EB`)
- **LED Glow**: Use `box-shadow` with LED RGB color
- **Pressed State**: Scale 0.95, Gray-500 background

### Animations
- **Key Press**: 100ms scale + background transition
- **LED Update**: 200ms color transition
- **Connection Status**: Fade in/out

### Responsive Design
- Desktop: Full keypad layout (15 keys visible)
- Tablet: Compact view (key list with LED indicators)
- Mobile: Vertical list (text labels only)

---

## Testing

### Test 1: Visual Key  Press
1. Press physical key on keypad
2. Verify dashboard key button animates (scales down, brightens)
3. Verify auto-release after 200ms
4. Verify key shows "K{keyId}" label

### Test 2: Connection Status
1. Disconnect keypad from CAN bus
2. Verify "Disconnected" indicator on dashboard
3. Reconnect keypad
4. Verify "Connected" indicator + firmware version displayed

### Test 3: Multiple Keys
1. Press multiple keys in rapid succession
2. Verify all keys animate independently
3. Verify no state conflicts

### Test 4: LED Visualization (Future)
1. When LED control implemented:
2. Verify dashboard shows LED color changes
3. Verify border glow matches RGB values

---

## Related Documentation

- **WebSocket Protocol**: `04-websocket-protocol-integration.md`
- **Userscript Integration**: `07-userscript-integration.md` (key mapping)
- **Composable Pattern**: Nuxt 4 composables documentation
- **UI Components**: nuxt-ui component library
- **Hardware Specs**: `../ots-hardware/modules/keypad-module.md`
