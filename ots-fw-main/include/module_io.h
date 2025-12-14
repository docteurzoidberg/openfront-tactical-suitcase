#pragma once

#include "io_expander.h"

// Hardware pin mapping for modules
// Maps module LEDs and buttons to specific MCP23017 board+pin combinations

// Pin mapping structure
struct PinMap {
  uint8_t board;  // MCP23017 board index (0-based)
  uint8_t pin;    // Pin number on that board (0-15)
};

// Main Power Module Pin Definitions
namespace MainModule {
  // LED outputs
  constexpr PinMap LED_LINK = {0, 0};  // Board 0, Pin 0 - Link status indicator
}

// Nuke Module Pin Definitions
namespace NukeModule {
  // Button inputs
  constexpr PinMap BTN_ATOM  = {0, 1};   // Board 0, Pin 1
  constexpr PinMap BTN_HYDRO = {0, 2};   // Board 0, Pin 2
  constexpr PinMap BTN_MIRV  = {0, 3};   // Board 0, Pin 3
  
  // LED outputs
  constexpr PinMap LED_ATOM  = {0, 8};   // Board 0, Pin 8
  constexpr PinMap LED_HYDRO = {0, 9};   // Board 0, Pin 9
  constexpr PinMap LED_MIRV  = {0, 10};  // Board 0, Pin 10
}

// Alert Module Pin Definitions
namespace AlertModule {
  // LED outputs
  constexpr PinMap LED_WARNING = {1, 0};  // Board 1, Pin 0
  constexpr PinMap LED_ATOM    = {1, 1};  // Board 1, Pin 1
  constexpr PinMap LED_HYDRO   = {1, 2};  // Board 1, Pin 2
  constexpr PinMap LED_MIRV    = {1, 3};  // Board 1, Pin 3
  constexpr PinMap LED_LAND    = {1, 4};  // Board 1, Pin 4
  constexpr PinMap LED_NAVAL   = {1, 5};  // Board 1, Pin 5
}

// Add more module namespaces here as needed

// Module I/O helper class
// High-level interface for reading inputs and controlling outputs
// using hardware-agnostic pin mappings

class ModuleIO {
public:
  explicit ModuleIO(IOExpander &ioExp) : io(ioExp) {}
  
  // Initialize all module pins
  bool begin();
  
  // Low-level pin operations
  bool readInput(const PinMap &pin, bool &state);
  bool writeOutput(const PinMap &pin, bool state);
  
  // LED control
  bool setLED(const PinMap &pin, bool on);
  bool toggleLED(const PinMap &pin);
  
  // Module-specific structures
  struct MainLEDs {
    bool link = false;
  };
  
  struct NukeButtons {
    bool atom = false;
    bool hydro = false;
    bool mirv = false;
  };
  
  struct NukeLEDs {
    bool atom = false;
    bool hydro = false;
    bool mirv = false;
  };
  
  struct AlertLEDs {
    bool warning = false;
    bool atom = false;
    bool hydro = false;
    bool mirv = false;
    bool land = false;
    bool naval = false;
  };
  
  // Module-specific batch operations
  bool writeMainLEDs(const MainLEDs &leds);
  NukeButtons readNukeButtons();
  bool writeNukeLEDs(const NukeLEDs &leds);
  bool writeAlertLEDs(const AlertLEDs &leds);

private:
  IOExpander &io;
  bool ledStates[MAX_MCP_BOARDS][16] = {{false}};
  
  // Helper to track LED state
  void updateLEDState(const PinMap &pin, bool state) {
    if (pin.board < MAX_MCP_BOARDS && pin.pin < 16) {
      ledStates[pin.board][pin.pin] = state;
    }
  }
  
  bool getLEDState(const PinMap &pin) const {
    if (pin.board < MAX_MCP_BOARDS && pin.pin < 16) {
      return ledStates[pin.board][pin.pin];
    }
    return false;
  }
  
  // Pin configuration helpers
  bool configurePinAsInput(const PinMap &pin);
  bool configurePinAsOutput(const PinMap &pin);
};
