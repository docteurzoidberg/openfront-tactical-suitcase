#pragma once

#include <Adafruit_MCP23X17.h>

// Maximum number of MCP23017 chips supported
#define MAX_MCP_BOARDS 8

// MCP23017 I/O Expander manager for OTS hardware modules
// Manages 1 to N MCP23017 chips on I2C bus with configurable addresses

class IOExpander {
public:
  IOExpander() : boardCount(0), initialized(false) {}
  
  // Initialize MCP23017 chips at specified addresses
  // Returns true if all boards initialized successfully
  bool begin(const uint8_t *addresses, uint8_t count);
  
  // Query methods
  uint8_t getBoardCount() const { return boardCount; }
  bool isInitialized() const { return initialized; }
  bool isValidBoard(uint8_t board) const { 
    return initialized && board < boardCount; 
  }
  
  // Pin configuration
  bool setPinMode(uint8_t board, uint8_t pin, uint8_t mode);
  
  // Digital I/O operations
  bool digitalWrite(uint8_t board, uint8_t pin, uint8_t value);
  bool digitalRead(uint8_t board, uint8_t pin, uint8_t &value);
  
  // Bulk operations (all 16 pins)
  bool readAll(uint8_t board, uint16_t &value);
  bool writeAll(uint8_t board, uint16_t value);

private:
  Adafruit_MCP23X17 boards[MAX_MCP_BOARDS];
  uint8_t boardCount;
  bool initialized;
  
  // Validate board and pin
  bool validateBoardPin(uint8_t board, uint8_t pin) const {
    return isValidBoard(board) && pin < 16;
  }
};
