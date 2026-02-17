# Userscript Integration - Keypad Module

This document outlines how the userscript handles keypad key mapping, configuration storage, and game action triggering.

## Overview

The **userscript** is responsible for ALL key mapping logic:
- Receives raw key events from WebSocket (K1-K15)
- Maps key IDs to game actions (stored in localStorage)
- Triggers game actions by simulating DOM clicks
- Provides configuration UI in new browser tab

**What userscript does:**
✅ Receives `KEYPAD_KEY_PRESSED/RELEASED` WebSocket events  
✅ Stores key bindings in browser localStorage  
✅ Provides configuration UI (Tampermonkey menu → new tab)  
✅ Triggers game actions by finding and clicking DOM elements  
✅ Handles export/import of configuration JSON  

**What userscript does NOT do:**
❌ No hardware control (firmware handles that)  
❌ No LED control (future feature in firmware)  
❌ No CAN bus communication (main controller handles that)  

---

## Default Key Bindings

The userscript provides these default bindings on first run:

**Row 1 (Building Actions):**
- K1 → Build City (keyboard: 1)
- K2 → Build Factory (keyboard: 2)
- K3 → Build Port (keyboard: 3)
- K4 → Build Defense Post (keyboard: 4)
- K5 → Build Missile Launcher (keyboard: 5)
- K6 → Build SAM (keyboard: 6)
- K7 → Build Warship (keyboard: 7)

**Row 2 (Game Controls):**
- K8 → Zoom In (keyboard: E)
- K9 → Zoom Out (keyboard: Q)
- K10 → Decrease Attack Ratio (keyboard: T)
- K11 → Switch Missile Direction (keyboard: U)
- K12 → Increase Attack Ratio (keyboard: Y)
- K13 → Boat Attack (keyboard: B)
- K14 → Land Attack (keyboard: G)

**Row 3 (View Control):**
- K15 → Toggle View (keyboard: Space)

**Note**: Nuke launches (keyboard 8/9/0) are intentionally NOT mapped - use dedicated Nuke Module hardware buttons instead.

---

## Architecture

### Component Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         Browser                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  Tab 1: OpenFront.io Game                                  │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │  Userscript (OTS Main)                               │  │ │
│  │  │  • WebSocket client → firmware                       │  │ │
│  │  │  • Listen: KEYPAD_KEY_PRESSED/RELEASED               │  │ │
│  │  │  • Lookup key binding in localStorage                │  │ │
│  │  │  • Trigger game action (DOM click)                   │  │ │
│  │  └──────────────────────────────────────────────────────┘  │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  Tab 2: OTS Keypad Config (userscript-generated)          │ │
│  │  • Visual 15-key interface (K1-K15)                       │ │
│  │  • Dropdown: Select game action per key                   │ │
│  │  • Save → write to localStorage                           │ │
│  │  • Export/Import buttons (JSON)                           │ │
│  │  • Reset to defaults button                               │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### Data Flow

```
Physical Keypress → Firmware → CAN → Main Controller → WebSocket
                                                            ↓
                        ┌───────────────────────────────────┘
                        ↓
                  Userscript (Tab 1)
                        ↓
                  localStorage lookup
                        ↓
                  Key Binding Found?
                    Yes ↓     No → Ignore
                  Trigger Game Action
                        ↓
                  DOM querySelector + click()
                        ↓
                  Game executes action
```

---

## localStorage Schema

### Key: `ots-keypad-bindings`

**Format**: JSON string

**Schema**:
```typescript
interface KeypadConfig {
  version: number;                    // Schema version (1)
  bindings: KeyBinding[];             // Array of 15 bindings (K1-K15)
}

interface KeyBinding {
  keyId: number;                      // 1-15
  action: string;                     // Game action identifier
  selector: string;                   // DOM selector for game button
  label?: string;                     // Optional user-friendly label
  enabled: boolean;                   // Enable/disable binding
}
```

**Example Data**:
```json
{
  "version": 1,
  "bindings": [
    {
      "keyId": 1,
      "action": "BUILD_CITY",
      "selector": "[data-hotkey='1']",
      "label": "City",
      "enabled": true
    },
    {
      "keyId": 2,
      "action": "BUILD_FACTORY",
      "selector": "[data-hotkey='2']",
      "label": "Factory",
      "enabled": true
    },
    {
      "keyId": 3,
      "action": "BUILD_PORT",
      "selector": "[data-hotkey='3']",
      "label": "Port",
      "enabled": true
    },
    {
      "keyId": 8,
      "action": "ZOOM_IN",
      "selector": "[data-hotkey='e']",
      "label": "Zoom+",
      "enabled": true
    },
    {
      "keyId": 13,
      "action": "BOAT_ATTACK",
      "selector": "[data-hotkey='b']",
      "label": "Naval",
      "enabled": true
    },
    {
      "keyId": 15,
      "action": "TOGGLE_VIEW",
      "selector": "[data-hotkey=' ']",
      "label": "View",
      "enabled": true
    }
    // ... 9 more bindings (K4-K7, K9-K12, K14)
  ]
}
```

### Default Bindings

**On first run**, create default config:
```typescript
const defaultBindings: KeyBinding[] = [
  // Row 1: Building actions
  { keyId: 1, action: 'BUILD_CITY', selector: '[data-hotkey="1"]', label: 'City', enabled: true },
  { keyId: 2, action: 'BUILD_FACTORY', selector: '[data-hotkey="2"]', label: 'Factory', enabled: true },
  { keyId: 3, action: 'BUILD_PORT', selector: '[data-hotkey="3"]', label: 'Port', enabled: true },
  { keyId: 4, action: 'BUILD_DEFENSE', selector: '[data-hotkey="4"]', label: 'Defense', enabled: true },
  { keyId: 5, action: 'BUILD_MISSILE', selector: '[data-hotkey="5"]', label: 'Missile', enabled: true },
  { keyId: 6, action: 'BUILD_SAM', selector: '[data-hotkey="6"]', label: 'SAM', enabled: true },
  { keyId: 7, action: 'BUILD_WARSHIP', selector: '[data-hotkey="7"]', label: 'Warship', enabled: true },
  
  // Row 2: Game controls
  { keyId: 8, action: 'ZOOM_IN', selector: '[data-hotkey="e"]', label: 'Zoom+', enabled: true },
  { keyId: 9, action: 'ZOOM_OUT', selector: '[data-hotkey="q"]', label: 'Zoom-', enabled: true },
  { keyId: 10, action: 'ATTACK_DECREASE', selector: '[data-hotkey="t"]', label: 'Atk-', enabled: true },
  { keyId: 11, action: 'MISSILE_SWITCH', selector: '[data-hotkey="u"]', label: 'Switch', enabled: true },
  { keyId: 12, action: 'ATTACK_INCREASE', selector: '[data-hotkey="y"]', label: 'Atk+', enabled: true },
  { keyId: 13, action: 'BOAT_ATTACK', selector: '[data-hotkey="b"]', label: 'Naval', enabled: true },
  { keyId: 14, action: 'LAND_ATTACK', selector: '[data-hotkey="g"]', label: 'Land', enabled: true },
  
  // Row 3: View control
  { keyId: 15, action: 'TOGGLE_VIEW', selector: '[data-hotkey=" "]', label: 'View', enabled: true }
];
```

---

## Userscript Implementation

### File Structure (ots-userscript/)

```
src/
  keypad/
    keypadManager.ts          - Main keypad logic
    keypadConfig.ts           - Configuration UI generator
    keypadStorage.ts          - localStorage helper
    keypadActions.ts          - Game action mapper
    types.ts                  - TypeScript interfaces
```

---

### 1. keypadManager.ts

**Purpose**: Receive WebSocket events and trigger game actions

```typescript
import { useWebSocket } from '../websocket';
import { getKeyBinding } from './keypadStorage';
import { triggerGameAction } from './keypadActions';

export class KeypadManager {
  private ws: WebSocket | null = null;
  
  constructor() {
    this.initWebSocket();
  }
  
  private initWebSocket() {
    this.ws = useWebSocket();
    
    this.ws.addEventListener('message', (event) => {
      const message = JSON.parse(event.data);
      
      if (message.event === 'KEYPAD_KEY_PRESSED') {
        this.handleKeyPressed(message.data);
      }
      
      if (message.event === 'KEYPAD_KEY_RELEASED') {
        this.handleKeyReleased(message.data);
      }
    });
  }
  
  private handleKeyPressed(data: { keyId: number; state: string; timestamp: number }) {
    console.log(`[OTS Keypad] Key ${data.keyId} pressed`);
    
    // Lookup binding in localStorage
    const binding = getKeyBinding(data.keyId);
    
    if (!binding || !binding.enabled) {
      console.log(`[OTS Keypad] Key ${data.keyId} not bound or disabled`);
      return;
    }
    
    // Trigger game action
    console.log(`[OTS Keypad] Triggering action: ${binding.action}`);
    triggerGameAction(binding);
  }
  
  private handleKeyReleased(data: { keyId: number; state: string; timestamp: number }) {
    console.log(`[OTS Keypad] Key ${data.keyId} released`);
    // Future: Handle hold-to-release actions
  }
}
```

---

### 2. keypadStorage.ts

**Purpose**: localStorage read/write helpers

```typescript
import type { KeypadConfig, KeyBinding } from './types';
import { defaultBindings } from './defaults';

const STORAGE_KEY = 'ots-keypad-bindings';
const SCHEMA_VERSION = 1;

export function loadConfig(): KeypadConfig {
  try {
    const json = localStorage.getItem(STORAGE_KEY);
    
    if (!json) {
      // First run: create defaults
      const config: KeypadConfig = {
        version: SCHEMA_VERSION,
        bindings: defaultBindings
      };
      saveConfig(config);
      return config;
    }
    
    const config: KeypadConfig = JSON.parse(json);
    
    // Validate schema version
    if (config.version !== SCHEMA_VERSION) {
      console.warn('[OTS Keypad] Config version mismatch, resetting to defaults');
      return resetConfig();
    }
    
    return config;
  } catch (error) {
    console.error('[OTS Keypad] Failed to load config:', error);
    return resetConfig();
  }
}

export function saveConfig(config: KeypadConfig): void {
  try {
    const json = JSON.stringify(config, null, 2);
    localStorage.setItem(STORAGE_KEY, json);
    console.log('[OTS Keypad] Config saved');
  } catch (error) {
    console.error('[OTS Keypad] Failed to save config:', error);
  }
}

export function getKeyBinding(keyId: number): KeyBinding | null {
  const config = loadConfig();
  return config.bindings.find(b => b.keyId === keyId) || null;
}

export function setKeyBinding(keyId: number, binding: Partial<KeyBinding>): void {
  const config = loadConfig();
  const index = config.bindings.findIndex(b => b.keyId === keyId);
  
  if (index >= 0) {
    config.bindings[index] = { ...config.bindings[index], ...binding };
    saveConfig(config);
  }
}

export function resetConfig(): KeypadConfig {
  const config: KeypadConfig = {
    version: SCHEMA_VERSION,
    bindings: defaultBindings
  };
  saveConfig(config);
  return config;
}

export function exportConfig(): string {
  const config = loadConfig();
  return JSON.stringify(config, null, 2);
}

export function importConfig(json: string): boolean {
  try {
    const config: KeypadConfig = JSON.parse(json);
    
    // Validate structure
    if (!config.version || !Array.isArray(config.bindings)) {
      throw new Error('Invalid config structure');
    }
    
    saveConfig(config);
    return true;
  } catch (error) {
    console.error('[OTS Keypad] Failed to import config:', error);
    return false;
  }
}
```

---

### 3. keypadActions.ts

**Purpose**: Trigger game actions by DOM manipulation

```typescript
import type { KeyBinding } from './types';

export function triggerGameAction(binding: KeyBinding): void {
  if (!binding.selector) {
    console.warn(`[OTS Keypad] No selector for action: ${binding.action}`);
    return;
  }
  
  try {
    // Find game button by selector
    const button = document.querySelector<HTMLElement>(binding.selector);
    
    if (!button) {
      console.error(`[OTS Keypad] Button not found: ${binding.selector}`);
      return;
    }
    
    // Check if button is disabled
    if (button.hasAttribute('disabled') || button.classList.contains('disabled')) {
      console.log(`[OTS Keypad] Button disabled: ${binding.action}`);
      return;
    }
    
    // Simulate click
    console.log(`[OTS Keypad] Clicking button: ${binding.selector}`);
    button.click();
    
    // Visual feedback (optional)
    button.style.outline = '2px solid #00ff00';
    setTimeout(() => {
      button.style.outline = '';
    }, 200);
    
  } catch (error) {
    console.error(`[OTS Keypad] Failed to trigger action: ${binding.action}`, error);
  }
}

export function validateSelector(selector: string): boolean {
  try {
    const element = document.querySelector(selector);
    return element !== null;
  } catch (error) {
    return false;
  }
}
```

---

### 4. keypadConfig.ts

**Purpose**: Generate configuration UI in new tab

```typescript
import { loadConfig, saveConfig, exportConfig, importConfig, resetConfig } from './keypadStorage';
import type { KeypadConfig } from './types';

export function openConfigTab(): void {
  // Open new window/tab
  const configWindow = window.open('', 'OTS Keypad Config', 'width=800,height=600');
  
  if (!configWindow) {
    alert('Failed to open config window. Check popup blocker.');
    return;
  }
  
  // Generate HTML
  const html = generateConfigHTML();
  configWindow.document.write(html);
  configWindow.document.close();
  
  // Attach event listeners
  attachConfigHandlers(configWindow);
}

function generateConfigHTML(): string {
  const config = loadConfig();
  
  return `
<!DOCTYPE html>
<html>
<head>
  <title>OTS Keypad Configuration</title>
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: #1a1a1a;
      color: #e5e7eb;
      padding: 2rem;
    }
    
    h1 {
      border-bottom: 2px solid #374151;
      padding-bottom: 1rem;
    }
    
    .keypad-grid {
      display: grid;
      grid-template-columns: repeat(7, 1fr);
      gap: 0.5rem;
      margin: 2rem 0;
    }
    
    .key-button {
      background: #374151;
      border: 2px solid #4b5563;
      border-radius: 0.5rem;
      padding: 1rem;
      cursor: pointer;
      transition: all 0.2s;
      text-align: center;
    }
    
    .key-button:hover {
      background: #4b5563;
      border-color: #6b7280;
    }
    
    .key-button.configured {
      border-color: #10b981;
    }
    
    .key-label {
      font-size: 1.5rem;
      font-weight: bold;
      margin-bottom: 0.5rem;
    }
    
    .key-action {
      font-size: 0.875rem;
      color: #9ca3af;
    }
    
    .key-spacebar {
      grid-column: span 4;
      grid-column-start: 2;
    }
    
    .config-panel {
      background: #374151;
      border-radius: 0.5rem;
      padding: 1.5rem;
      margin-top: 2rem;
      display: none;
    }
    
    .config-panel.active {
      display: block;
    }
    
    .form-group {
      margin-bottom: 1rem;
    }
    
    label {
      display: block;
      margin-bottom: 0.5rem;
      font-weight: 500;
    }
    
    select, input {
      width: 100%;
      padding: 0.5rem;
      background: #1f2937;
      border: 1px solid #4b5563;
      border-radius: 0.25rem;
      color: #e5e7eb;
    }
    
    .button-group {
      display: flex;
      gap: 0.5rem;
      margin-top: 2rem;
    }
    
    button {
      padding: 0.75rem 1.5rem;
      border: none;
      border-radius: 0.25rem;
      cursor: pointer;
      font-weight: 500;
      transition: all 0.2s;
    }
    
    button.primary {
      background: #3b82f6;
      color: white;
    }
    
    button.primary:hover {
      background: #2563eb;
    }
    
    button.secondary {
      background: #6b7280;
      color: white;
    }
    
    button.secondary:hover {
      background: #4b5563;
    }
    
    button.danger {
      background: #ef4444;
      color: white;
    }
    
    button.danger:hover {
      background: #dc2626;
    }
  </style>
</head>
<body>
  <h1>OTS Keypad Configuration</h1>
  
  <div class="keypad-grid">
    ${generateKeyButtons(config)}
  </div>
  
  <div id="configPanel" class="config-panel">
    <h3>Configure Key <span id="configKeyId"></span></h3>
    
    <div class="form-group">
      <label for="actionSelect">Game Action:</label>
      <select id="actionSelect">
        <option value="NONE">None (Unassigned)</option>
        <optgroup label="Building Actions">
          <option value="BUILD_CITY">Build City (1)</option>
          <option value="BUILD_FACTORY">Build Factory (2)</option>
          <option value="BUILD_PORT">Build Port (3)</option>
          <option value="BUILD_DEFENSE">Build Defense Post (4)</option>
          <option value="BUILD_MISSILE">Build Missile Launcher (5)</option>
          <option value="BUILD_SAM">Build SAM (6)</option>
          <option value="BUILD_WARSHIP">Build Warship (7)</option>
        </optgroup>
        <optgroup label="Game Controls">
          <option value="ZOOM_IN">Zoom In (E)</option>
          <option value="ZOOM_OUT">Zoom Out (Q)</option>
          <option value="ATTACK_DECREASE">Decrease Attack Ratio (T)</option>
          <option value="MISSILE_SWITCH">Switch Missile Direction (U)</option>
          <option value="ATTACK_INCREASE">Increase Attack Ratio (Y)</option>
          <option value="BOAT_ATTACK">Boat Attack (B)</option>
          <option value="LAND_ATTACK">Land Attack (G)</option>
        </optgroup>
        <optgroup label="View Control">
          <option value="TOGGLE_VIEW">Toggle View (Space)</option>
        </optgroup>
        <option value="CUSTOM">Custom DOM Selector...</option>
      </select>
    </div>
    
    <div class="form-group">
      <label for="selectorInput">DOM Selector:</label>
      <input id="selectorInput" type="text" placeholder="#button-id or .button-class" />
    </div>
    
    <div class="form-group">
      <label for="labelInput">Label (optional):</label>
      <input id="labelInput" type="text" placeholder="Custom label" maxlength="16" />
    </div>
    
    <div class="button-group">
      <button class="secondary" onclick="closeConfig()">Cancel</button>
      <button class="primary" onclick="saveBinding()">Save</button>
    </div>
  </div>
  
  <div class="button-group">
    <button class="secondary" onclick="exportBindings()">Export JSON</button>
    <button class="secondary" onclick="importBindings()">Import JSON</button>
    <button class="danger" onclick="resetAllBindings()">Reset to Defaults</button>
  </div>
  
  <script>
    // Will be injected by attachConfigHandlers()
  </script>
</body>
</html>
  `;
}

function generateKeyButtons(config: KeypadConfig): string {
  return config.bindings.map(binding => {
    const isSpacebar = binding.keyId === 15;
    const configured = binding.enabled && binding.action !== 'NONE';
    
    return `
      <div class="key-button ${configured ? 'configured' : ''} ${isSpacebar ? 'key-spacebar' : ''}"
           onclick="openBinding(${binding.keyId})">
        <div class="key-label">K${binding.keyId}</div>
        <div class="key-action">${binding.label || 'Unassigned'}</div>
      </div>
    `;
  }).join('');
}

function attachConfigHandlers(configWindow: Window): void {
  // Inject JavaScript functions into config window
  (configWindow as any).openBinding = (keyId: number) => {
    const config = loadConfig();
    const binding = config.bindings.find(b => b.keyId === keyId);
    
    if (!binding) return;
    
    const panel = configWindow.document.getElementById('configPanel');
    const keyIdSpan = configWindow.document.getElementById('configKeyId');
    const actionSelect = configWindow.document.getElementById('actionSelect') as HTMLSelectElement;
    const selectorInput = configWindow.document.getElementById('selectorInput') as HTMLInputElement;
    const labelInput = configWindow.document.getElementById('labelInput') as HTMLInputElement;
    
    if (!panel || !keyIdSpan || !actionSelect || !selectorInput || !labelInput) return;
    
    // Populate form
    keyIdSpan.textContent = String(keyId);
    actionSelect.value = binding.action;
    selectorInput.value = binding.selector || '';
    labelInput.value = binding.label || '';
    
    // Show panel
    panel.classList.add('active');
    (configWindow as any).currentKeyId = keyId;
  };
  
  (configWindow as any).closeConfig = () => {
    const panel = configWindow.document.getElementById('configPanel');
    if (panel) panel.classList.remove('active');
  };
  
  (configWindow as any).saveBinding = () => {
    const keyId = (configWindow as any).currentKeyId;
    const actionSelect = configWindow.document.getElementById('actionSelect') as HTMLSelectElement;
    const selectorInput = configWindow.document.getElementById('selectorInput') as HTMLInputElement;
    const labelInput = configWindow.document.getElementById('labelInput') as HTMLInputElement;
    
    const action = actionSelect.value;
    const selector = selectorInput.value;
    const label = labelInput.value;
    
    // Update config
    const config = loadConfig();
    const binding = config.bindings.find(b => b.keyId === keyId);
    
    if (binding) {
      binding.action = action;
      binding.selector = selector;
      binding.label = label || `K${keyId}`;
      binding.enabled = action !== 'NONE';
      
      saveConfig(config);
      alert('Binding saved! Reloading page...');
      configWindow.location.reload();
    }
  };
  
  (configWindow as any).exportBindings = () => {
    const json = exportConfig();
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = configWindow.document.createElement('a');
    a.href = url;
    a.download = 'ots-keypad-bindings.json';
    a.click();
  };
  
  (configWindow as any).importBindings = () => {
    const input = configWindow.document.createElement('input');
    input.type = 'file';
    input.accept = 'application/json';
    
    input.onchange = (e: Event) => {
      const file = (e.target as HTMLInputElement).files?.[0];
      if (!file) return;
      
      const reader = new FileReader();
      reader.onload = (event) => {
        const json = event.target?.result as string;
        if (importConfig(json)) {
          alert('Config imported successfully!');
          configWindow.location.reload();
        } else {
          alert('Failed to import config. Check console for errors.');
        }
      };
      reader.readAsText(file);
    };
    
    input.click();
  };
  
  (configWindow as any).resetAllBindings = () => {
    if (confirm('Reset all bindings to defaults?')) {
      resetConfig();
      alert('Config reset!');
      configWindow.location.reload();
    }
  };
}
```

---

## Tampermonkey Menu Integration

**File**: `src/main.user.ts`

Add menu command:
```typescript
import { openConfigTab } from './keypad/keypadConfig';

GM_registerMenuCommand('OTS Keypad Config', () => {
  openConfigTab();
});
```

---

## Game Action Selectors

**Action Enum → DOM Selectors Mapping**

These selectors assume OpenFront.io uses `data-hotkey` attributes for keyboard bindings:

```typescript
export const gameActionSelectors = {
  // Building actions (Row 1)
  BUILD_CITY: '[data-hotkey="1"]',
  BUILD_FACTORY: '[data-hotkey="2"]',
  BUILD_PORT: '[data-hotkey="3"]',
  BUILD_DEFENSE: '[data-hotkey="4"]',
  BUILD_MISSILE: '[data-hotkey="5"]',
  BUILD_SAM: '[data-hotkey="6"]',
  BUILD_WARSHIP: '[data-hotkey="7"]',
  
  // Game controls (Row 2)
  ZOOM_IN: '[data-hotkey="e"]',
  ZOOM_OUT: '[data-hotkey="q"]',
  ATTACK_DECREASE: '[data-hotkey="t"]',
  MISSILE_SWITCH: '[data-hotkey="u"]',
  ATTACK_INCREASE: '[data-hotkey="y"]',
  BOAT_ATTACK: '[data-hotkey="b"]',
  LAND_ATTACK: '[data-hotkey="g"]',
  
  // View control (Row 3)
  TOGGLE_VIEW: '[data-hotkey=" "]',  // Space bar
};
```

**Note**: Actual selectors must be verified by inspecting OpenFront.io DOM. If `data-hotkey` attributes are not present, selectors may need to target button IDs, classes, or use keyboard event simulation instead of DOM clicks.

---

## Testing

### Test 1: Configuration UI
1. Install userscript in Tampermonkey
2. Open OpenFront.io
3. Click Tampermonkey icon → "OTS Keypad Config"
4. Verify new tab opens with 15-key grid
5. Click key → verify config panel opens
6. Select action → verify dropdown works
7. Save → verify localStorage updated

### Test 2: Key Event Handling
1. Default config: K1 = "Build City" (keyboard 1)
2. Press physical key 1 on keypad
3. Verify userscript receives `KEYPAD_KEY_PRESSED` event with keyId=1
4. Verify console log: "Triggering action: BUILD_CITY"
5. Verify game button with [data-hotkey="1"] clicked
6. Verify city build action executed in game

### Test 3: Export/Import
1. Configure multiple keys
2. Click "Export JSON"
3. Verify JSON file downloaded
4. Clear localStorage
5. Click "Import JSON"
6. Verify bindings restored

### Test 4: Default Bindings
1. Clear localStorage (simulate first run)
2. Reload page
3. Open config UI
4. Verify all 15 default bindings loaded:
   - K1-K7: Building actions
   - K8-K14: Game controls
   - K15: Toggle view
5. Press each key and verify correct game action triggered

---

## Future Enhancements

### Phase 2: Advanced Features
- **Macro support**: Bind key to sequence of actions (e.g., K1 = nuke + deploy)
- **Hold-to-release actions**: Different action on key hold vs tap
- **Conditional bindings**: Different actions based on game state (e.g., if nuclear weapons available)
- **Profile switching**: Load different binding sets per game mode

### Phase 3: LED Feedback
- Userscript sends LED commands to firmware via WebSocket
- LED color reflects game state (green = available, red = cooldown)
- Per-key LED configuration in config UI

---

## Related Documentation

- **WebSocket Protocol**: `04-websocket-protocol-integration.md`
- **Dashboard UI**: `05-dashboard-ui-integration.md` (visualization only)
- **Main Controller**: `02-main-controller-integration.md` (event forwarding)
- **Firmware Architecture**: `01-keypad-firmware-architecture.md`
