/**
 * @file sdcard.c
 * @brief SD Card Hardware Abstraction Layer Implementation
 */

#include "sdcard.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include <string.h>

static const char *TAG = "sdcard";

static sdmmc_card_t *s_card = NULL;
static bool s_mounted = false;

esp_err_t sdcard_init(void)
{
    ESP_LOGI(TAG, "Initializing SD card in SPI mode...");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_host_device_t spi_host = SPI2_HOST;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_CARD_MOSI,
        .miso_io_num = SD_CARD_MISO,
        .sclk_io_num = SD_CARD_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    esp_err_t ret = spi_bus_initialize(spi_host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CARD_CS;
    slot_config.host_id = spi_host;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024
    };

    ret = esp_vfs_fat_sdspi_mount(SD_CARD_MOUNT_POINT, &host,
                                   &slot_config, &mount_cfg, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card FATFS: %s", esp_err_to_name(ret));
        s_mounted = false;
        return ret;
    }

    s_mounted = true;
    ESP_LOGI(TAG, "SD card mounted at %s", SD_CARD_MOUNT_POINT);
    sdmmc_card_print_info(stdout, s_card);
    
    return ESP_OK;
}

bool sdcard_is_mounted(void)
{
    return s_mounted;
}

esp_err_t sdcard_unmount(void)
{
    if (!s_mounted) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Unmounting SD card...");
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_CARD_MOUNT_POINT, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_mounted = false;
    s_card = NULL;
    ESP_LOGI(TAG, "SD card unmounted");
    
    return ESP_OK;
}
