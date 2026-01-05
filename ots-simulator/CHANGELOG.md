# Changelog

All notable changes to the OTS Server project will be documented in this file.

## [Unreleased]

### Added
- **Troops Module Component**: New hardware module component with realistic LCD display simulation
  - I2C 1602A APKLVSR yellow-green LCD display with character-by-character rendering
  - 16-character fixed-width display with monospace font and tabular numbers
  - Slider control for troop deployment percentage (0-100%)
  - Real-time troop calculation display (current troops × percent)
  - Unit formatting with K/M/B suffixes for large numbers
  - Debounced slider input (100ms) to prevent command spam
  - Integration with WebSocket for `set-troops-percent` command
  
- **Enhanced Event Log System**: 
  - Custom alert messages with detailed information for each alert type
    - Nuclear alerts (ATOM/HYDRO/MIRV) show launcher name and target coordinates
    - Invasion alerts (LAND/NAVAL) show attacker name and troop count
  - Color-coded event badges by category:
    - Red for nuclear alerts (ATOM/HYDRO/MIRV)
    - Orange for land invasions
    - Blue for naval invasions
    - Yellow for nuke events (exploded/intercepted)
  - Fixed-height scrollable container with overflow handling
  - Reversed event order (oldest to newest, top to bottom)
  - Auto-scroll toggle button in header
    - Automatically scrolls to show new events when enabled
    - Can be disabled to review historical events
  - Slide-in animation for newly added events
    - 0.6s smooth animation with cyan highlight flash
    - Events slide in from left with fade-in effect

### Changed
- **Dashboard Layout**: Reorganized to 3-column grid layout
  - Column 1: Main Power Module + Troops Module
  - Column 2: Alert Module + Nuke Module
  - Column 3: Event Log (sticky, full height)
  - Improved visual organization by functional grouping
  
- **LCD Display Styling**: Enhanced visual realism
  - Yellow-green gradient background matching I2C 1602A APKLVSR hardware
  - Bezel effect with shadow and border
  - Character-level rendering for authentic LCD appearance
  - Centered LCD display within module (slider remains full width)
  
- **Event Badge Colors**: Updated to match alert severity and type
  - More intuitive color scheme for quick alert identification

### Fixed
- Event log scrolling now works correctly with proper container structure
- LCD character width precisely set to 16 characters
- Auto-scroll behavior synchronized with DOM updates using nextTick

## Project Information

**Tech Stack:**
- Nuxt 3 (Vue 3 Composition API)
- Nuxt UI components
- TailwindCSS
- TypeScript
- WebSocket (real-time communication)

**Protocol Support:**
- State messages with troops data
- Command messages (`set-troops-percent`)
- Event messages (alerts, nukes, game state)
