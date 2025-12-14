#include "module_io.h"
#include <Arduino.h>

bool ModuleIO::configurePinAsInput(const PinMap &pin) {
  return io.setPinMode(pin.board, pin.pin, INPUT_PULLUP);
}

bool ModuleIO::configurePinAsOutput(const PinMap &pin) {
  return io.setPinMode(pin.board, pin.pin, OUTPUT);
}

bool ModuleIO::begin() {
  if (!io.isInitialized()) {
    Serial.println("[ModuleIO] ERROR: IOExpander not initialized!");
    return false;
  }
  
  Serial.println("[ModuleIO] Configuring pins...");
  
  // Configure Main Module
  configurePinAsOutput(MainModule::LED_LINK);
  
  // Configure Nuke Module
  configurePinAsInput(NukeModule::BTN_ATOM);
  configurePinAsInput(NukeModule::BTN_HYDRO);
  configurePinAsInput(NukeModule::BTN_MIRV);
  
  configurePinAsOutput(NukeModule::LED_ATOM);
  configurePinAsOutput(NukeModule::LED_HYDRO);
  configurePinAsOutput(NukeModule::LED_MIRV);
  
  // Configure Alert Module (if available)
  if (io.isValidBoard(AlertModule::LED_WARNING.board)) {
    configurePinAsOutput(AlertModule::LED_WARNING);
    configurePinAsOutput(AlertModule::LED_ATOM);
    configurePinAsOutput(AlertModule::LED_HYDRO);
    configurePinAsOutput(AlertModule::LED_MIRV);
    configurePinAsOutput(AlertModule::LED_LAND);
    configurePinAsOutput(AlertModule::LED_NAVAL);
  }
  
  // Initialize LEDs to OFF
  writeMainLEDs(MainLEDs{});
  writeNukeLEDs(NukeLEDs{});
  if (io.isValidBoard(AlertModule::LED_WARNING.board)) {
    writeAlertLEDs(AlertLEDs{});
  }
  
  Serial.println("[ModuleIO] Configuration complete");
  return true;
}

bool ModuleIO::readInput(const PinMap &pin, bool &state) {
  uint8_t value;
  if (!io.digitalRead(pin.board, pin.pin, value)) {
    state = false;
    return false;
  }
  // Invert for active-low buttons with pull-up
  state = !value;
  return true;
}

bool ModuleIO::writeOutput(const PinMap &pin, bool state) {
  if (!io.digitalWrite(pin.board, pin.pin, state ? HIGH : LOW)) {
    return false;
  }
  updateLEDState(pin, state);
  return true;
}

bool ModuleIO::setLED(const PinMap &pin, bool on) {
  return writeOutput(pin, on);
}

bool ModuleIO::toggleLED(const PinMap &pin) {
  return writeOutput(pin, !getLEDState(pin));
}

ModuleIO::NukeButtons ModuleIO::readNukeButtons() {
  NukeButtons buttons;
  readInput(NukeModule::BTN_ATOM, buttons.atom);
  readInput(NukeModule::BTN_HYDRO, buttons.hydro);
  readInput(NukeModule::BTN_MIRV, buttons.mirv);
  return buttons;
}

bool ModuleIO::writeMainLEDs(const MainLEDs &leds) {
  return writeOutput(MainModule::LED_LINK, leds.link);
}

bool ModuleIO::writeNukeLEDs(const NukeLEDs &leds) {
  bool success = true;
  success &= writeOutput(NukeModule::LED_ATOM, leds.atom);
  success &= writeOutput(NukeModule::LED_HYDRO, leds.hydro);
  success &= writeOutput(NukeModule::LED_MIRV, leds.mirv);
  return success;
}

bool ModuleIO::writeAlertLEDs(const AlertLEDs &leds) {
  bool success = true;
  success &= writeOutput(AlertModule::LED_WARNING, leds.warning);
  success &= writeOutput(AlertModule::LED_ATOM, leds.atom);
  success &= writeOutput(AlertModule::LED_HYDRO, leds.hydro);
  success &= writeOutput(AlertModule::LED_MIRV, leds.mirv);
  success &= writeOutput(AlertModule::LED_LAND, leds.land);
  success &= writeOutput(AlertModule::LED_NAVAL, leds.naval);
  return success;
}
