/**
 * @file board_config.h
 * @brief ESP32-A1S Board Configuration
 * 
 * Pin definitions and hardware configuration for the ESP32-A1S Audio Kit
 * (Ai-Thinker / trombik variant)
 */

#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------------
 *  CAN Bus Configuration
 *-----------------------------------------------------------------------*/
#define CAN_TX_GPIO    GPIO_NUM_21
#define CAN_RX_GPIO    GPIO_NUM_22
#define CAN_BITRATE    500000  // 500 kbps

/*------------------------------------------------------------------------
 *  I2C Configuration
 *-----------------------------------------------------------------------*/
#define I2C_PORT       I2C_NUM_0
#ifndef I2C_SDA_IO
#define I2C_SDA_IO     GPIO_NUM_33
#endif
#ifndef I2C_SCL_IO
#define I2C_SCL_IO     GPIO_NUM_32
#endif
#define I2C_FREQ_HZ    100000

/*------------------------------------------------------------------------
 *  I2S Configuration
 *-----------------------------------------------------------------------*/
#define I2S_PORT       I2S_NUM_0
#ifndef I2S_BCK_IO
#define I2S_BCK_IO     GPIO_NUM_27  // Bit clock
#endif
#ifndef I2S_WS_IO
#define I2S_WS_IO      GPIO_NUM_25  // Word select (LRCK)
#endif
#ifndef I2S_DO_IO
#define I2S_DO_IO      GPIO_NUM_26  // Data out
#endif
#ifndef I2S_DI_IO
#define I2S_DI_IO      GPIO_NUM_35  // Data in (not used for playback)
#endif
#ifndef I2S_MCLK_IO
#define I2S_MCLK_IO    GPIO_NUM_0   // Master clock
#endif

/*------------------------------------------------------------------------
 *  SD Card Configuration (SPI mode)
 *-----------------------------------------------------------------------*/
#ifndef SD_CARD_CS
#define SD_CARD_CS     GPIO_NUM_13
#endif
#ifndef SD_CARD_MISO
#define SD_CARD_MISO   GPIO_NUM_2
#endif
#ifndef SD_CARD_MOSI
#define SD_CARD_MOSI   GPIO_NUM_15
#endif
#ifndef SD_CARD_SCK
#define SD_CARD_SCK    GPIO_NUM_14
#endif

/*------------------------------------------------------------------------
 *  Audio Configuration
 *-----------------------------------------------------------------------*/
#define DEFAULT_SAMPLE_RATE  44100  // Default sample rate (Hz)

#ifdef __cplusplus
}
#endif
