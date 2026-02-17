/**
 * @file config.h
 * @brief OTS Keypad Module Configuration
 * 
 * Hardware configuration and GPIO pin assignments for the keypad module.
 * Based on PCB schematic for M5Stack Stamp S3.
 */

#ifndef CONFIG_H
#define CONFIG_H

/* Firmware Version */
#define OTS_KEYPAD_VERSION "0.1.0-dev"

/* ============================================================================
 * GPIO PIN ASSIGNMENTS (from PCB schematic)
 * ============================================================================ */

/* Key Matrix Row Pins (Outputs, Active High) */
#define GPIO_ROW0   6
#define GPIO_ROW1   7
#define GPIO_ROW2   8

/* Key Matrix Column Pins (Inputs with Pull-ups) */
#define GPIO_COL0   9
#define GPIO_COL1   10
#define GPIO_COL2   11
#define GPIO_COL3   12
#define GPIO_COL4   13
#define GPIO_COL5   14
#define GPIO_COL6   21

/* RGB LED Control */
#define GPIO_RGB_DATA   3   // SK6812-MINI-E data line

/* CAN Bus Interface */
#define GPIO_CAN_TX     5   // To TJA1050/MCP2551
#define GPIO_CAN_RX     4   // From TJA1050/MCP2551

/* ============================================================================
 * MATRIX CONFIGURATION
 * ============================================================================ */

#define MATRIX_ROWS     3
#define MATRIX_COLS     7
#define NUM_KEYS        15

/* Key Matrix Layout:
 * 
 *         Col0  Col1  Col2  Col3  Col4  Col5  Col6
 * Row0:    K1    K2    K3    K4    K5    K6    K7
 * Row1:    K8    K9    K10   K11   K12   K13   K14
 * Row2:    --    --    --    K15   --    --    --
 * 
 * Key 15 (spacebar) is at Row2, Col3
 */

#define KEYPAD_DEBOUNCE_MS      20      // Key debounce time in milliseconds
#define KEYPAD_SCAN_RATE_MS     5       // Matrix scan interval

/* ============================================================================
 * RGB LED CONFIGURATION
 * ============================================================================ */

#define NUM_LEDS                15      // One LED per key
#define LED_BRIGHTNESS_DEFAULT  128     // 0-255 (50% brightness)
#define LED_BRIGHTNESS_MAX      255     // Maximum brightness

/* SK6812-MINI-E Timing (WS2812-compatible) */
#define LED_RMT_CHANNEL         0       // RMT channel for LED control

/* ============================================================================
 * CAN BUS CONFIGURATION
 * ============================================================================ */

#define CAN_BITRATE             500000  // 500 kbit/s
#define CAN_TX_QUEUE_SIZE       10
#define CAN_RX_QUEUE_SIZE       10

/* CAN Module ID (to be assigned during discovery) */
#define CAN_MODULE_TYPE_KEYPAD  0x05    // Module type identifier

/* CAN Message IDs */
#define CAN_ID_KEY_EVENT_BASE   0x200   // 0x200 + key_id (0x201-0x20F)
#define CAN_ID_LED_SET          0x210   // Single key LED control
#define CAN_ID_LED_BULK         0x211   // Bulk LED control (bitmask)

/* ============================================================================
 * USB HID CONFIGURATION
 * ============================================================================ */

#define USB_HID_ENABLED         1       // Enable USB HID keyboard mode
#define USB_VID                 0x1209  // PID.codes test VID
#define USB_PID                 0x0001  // Placeholder PID (update in production)

#endif /* CONFIG_H */
