#include "io_expander.h"
#include <Arduino.h>

bool IOExpander::begin(const uint8_t *addresses, uint8_t count) {
  // Validate input
  if (!addresses || count == 0 || count > MAX_MCP_BOARDS) {
    Serial.printf("[IO] ERROR: Invalid parameters (count=%d, max=%d)\n", 
                  count, MAX_MCP_BOARDS);
    return false;
  }
  
  Serial.printf("[IO] Initializing %d MCP23017(s)...\n", count);
  
  boardCount = 0;
  bool allSuccess = true;
  
  // Initialize each board
  for (uint8_t i = 0; i < count; i++) {
    uint8_t addr = addresses[i];
    
    if (!boards[i].begin_I2C(addr)) {
      Serial.printf("[IO] ERROR: Board #%d failed at 0x%02X\n", i, addr);
      allSuccess = false;
      continue;
    }
    
    Serial.printf("[IO] Board #%d ready at 0x%02X\n", i, addr);
    boardCount++;
  }
  
  // Check if at least one board initialized
  if (boardCount == 0) {
    Serial.println("[IO] ERROR: No boards initialized!");
    initialized = false;
    return false;
  }
  
  initialized = true;
  Serial.printf("[IO] Ready: %d/%d board(s)\n", boardCount, count);
  return allSuccess;
}

bool IOExpander::setPinMode(uint8_t board, uint8_t pin, uint8_t mode) {
  if (!validateBoardPin(board, pin)) return false;
  boards[board].pinMode(pin, mode);
  return true;
}

bool IOExpander::digitalWrite(uint8_t board, uint8_t pin, uint8_t value) {
  if (!validateBoardPin(board, pin)) return false;
  boards[board].digitalWrite(pin, value);
  return true;
}

bool IOExpander::digitalRead(uint8_t board, uint8_t pin, uint8_t &value) {
  if (!validateBoardPin(board, pin)) {
    value = 0;
    return false;
  }
  value = boards[board].digitalRead(pin);
  return true;
}

bool IOExpander::readAll(uint8_t board, uint16_t &value) {
  if (!isValidBoard(board)) {
    value = 0;
    return false;
  }
  value = boards[board].readGPIOAB();
  return true;
}

bool IOExpander::writeAll(uint8_t board, uint16_t value) {
  if (!isValidBoard(board)) return false;
  boards[board].writeGPIOAB(value);
  return true;
}
