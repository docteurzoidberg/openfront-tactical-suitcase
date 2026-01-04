/**
 * @file sdcard.h
 * @brief SD Card Hardware Abstraction Layer
 * 
 * Manages SD card mounting via SPI interface
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// SD card SPI pins
#ifndef SD_CARD_CS
#define SD_CARD_CS   GPIO_NUM_13
#endif
#ifndef SD_CARD_MOSI
#define SD_CARD_MOSI GPIO_NUM_15
#endif
#ifndef SD_CARD_MISO
#define SD_CARD_MISO GPIO_NUM_2
#endif
#ifndef SD_CARD_SCK
#define SD_CARD_SCK  GPIO_NUM_14
#endif

// Mount point
#define SD_CARD_MOUNT_POINT "/sdcard"

/**
 * @brief Initialize and mount SD card
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sdcard_init(void);

/**
 * @brief Check if SD card is mounted
 * @return true if mounted, false otherwise
 */
bool sdcard_is_mounted(void);

/**
 * @brief Unmount SD card
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sdcard_unmount(void);

#ifdef __cplusplus
}
#endif
