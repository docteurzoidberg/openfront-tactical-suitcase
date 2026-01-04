/**
 * @file board_config.h
 * @brief ESP32-A1S board pin definitions
 */

#pragma once

// I2S pins (FIXED: BCK must be GPIO 27, not GPIO 5!)
#define I2S_MCLK_IO     0
#define I2S_BCK_IO      27
#define I2S_WS_IO       25
#define I2S_DO_IO       26
#define I2S_DI_IO       35

// I2C pins (for ES8388)
#define I2C_SDA_IO      33
#define I2C_SCL_IO      32
#define I2C_MASTER_NUM  I2C_NUM_0
#define I2C_MASTER_FREQ 100000

// ES8388 I2C address
#define ES8388_ADDR     0x10

// Power amplifier enable
#define PA_ENABLE_GPIO  21
