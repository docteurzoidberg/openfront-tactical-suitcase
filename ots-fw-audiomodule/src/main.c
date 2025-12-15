#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "driver/i2s.h"
#include "driver/i2c.h"

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "MAIN";

/*------------------------------------------------------------------------
 *  Table commandes série -> nom de fichier WAV
 *-----------------------------------------------------------------------*/

typedef struct {
    const char *cmd;      // ce que tu tapes sur le port série (sans \n)
    const char *filename; // dans /sdcard
} command_entry_t;

static const command_entry_t s_command_table[] = {
    { "1",      "track1.wav" },
    { "2",      "track2.wav" },
    { "HELLO",  "hello.wav"  },
    { "PING",   "ping.wav"   },
};
static const size_t s_command_table_len =
    sizeof(s_command_table) / sizeof(s_command_table[0]);

/*------------------------------------------------------------------------
 *  Pins SD & I2S (Ai-Thinker / trombik)
 *-----------------------------------------------------------------------*/

#ifndef SD_CARD_CS
#define SD_CARD_CS   13
#define SD_CARD_MISO 2
#define SD_CARD_MOSI 15
#define SD_CARD_SCK  14
#endif

#ifndef I2S_BCK_IO
#define I2S_BCK_IO   27
#define I2S_WS_IO    25
#define I2S_DO_IO    26
#define I2S_DI_IO    35
#define I2S_MCLK_IO  0
#endif

#ifndef I2C_SDA_IO
#define I2C_SDA_IO   33
#define I2C_SCL_IO   32
#endif

#define I2C_PORT     I2C_NUM_0
#define I2C_FREQ_HZ  100000
#define ES8388_ADDR  0x10   // valeur habituelle sur cette carte

#define I2S_PORT     I2S_NUM_0

/*------------------------------------------------------------------------
 *  Infos WAV
 *-----------------------------------------------------------------------*/

typedef struct {
    uint32_t sample_rate;
    uint16_t num_channels;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;

/*------------------------------------------------------------------------
 *  Globals
 *-----------------------------------------------------------------------*/

static sdmmc_card_t *s_sdcard = NULL;

/*------------------------------------------------------------------------
 *  Helpers
 *-----------------------------------------------------------------------*/

static void trim_newline(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/*------------------------------------------------------------------------
 *  SD card (SPI + FATFS)
 *-----------------------------------------------------------------------*/

static esp_err_t init_sdcard(void)
{
    ESP_LOGI(TAG, "Initializing SD card in SPI mode...");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SDSPI_HOST_SLOT_1;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_CARD_MOSI,
        .miso_io_num = SD_CARD_MISO,
        .sclk_io_num = SD_CARD_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO));

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CARD_CS;
    slot_config.host_id = host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024
    };

    const char mount_point[] = "/sdcard";

    esp_err_t ret =
        esp_vfs_fat_sdspi_mount(mount_point, &host,
                                &slot_config, &mount_cfg, &s_sdcard);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card FATFS (%s)",
                 esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted at %s", mount_point);
    sdmmc_card_print_info(stdout, s_sdcard);
    return ESP_OK;
}

/*------------------------------------------------------------------------
 *  I2S
 *-----------------------------------------------------------------------*/

static esp_err_t init_i2s_default(uint32_t sample_rate)
{
    ESP_LOGI(TAG, "Initializing I2S @ %lu Hz",
             (unsigned long)sample_rate);

    i2s_config_t i2s_cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256
    };

    i2s_pin_config_t pin_cfg = {
        .bck_io_num = I2S_BCK_IO,
        .ws_io_num = I2S_WS_IO,
        .data_out_num = I2S_DO_IO,
        .data_in_num = I2S_DI_IO
    };

    ESP_ERROR_CHECK(i2s_driver_install(I2S_PORT, &i2s_cfg, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_PORT, &pin_cfg));
    ESP_ERROR_CHECK(i2s_set_clk(I2S_PORT, sample_rate,
                                I2S_BITS_PER_SAMPLE_16BIT,
                                I2S_CHANNEL_STEREO));
    return ESP_OK;
}

/*------------------------------------------------------------------------
 *  I2C + Codec ES8388 (crochet vers trombik / ESP-ADF)
 *-----------------------------------------------------------------------*/

static esp_err_t init_i2c(void)
{
    ESP_LOGI(TAG, "Initializing I2C for codec (ES8388 @ 0x%02X)",
             ES8388_ADDR);

    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_IO,
        .scl_io_num = I2C_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
        .clk_flags = 0
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &cfg));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, cfg.mode, 0, 0, 0));
    return ESP_OK;
}

/*
 * Ici tu peux brancher le driver ES8388 de l’ESP-ADF + board trombik:
 *
 *   - clone esp-adf (ADF_PATH) et le composant trombik dans components/
 *   - inclure "audio_board.h", "audio_hal.h", "es8388.h"
 *   - utiliser audio_board_init() / audio_hal_ctrl_codec(), etc.
 *
 * Le skeleton se contente de fournir le point d’entrée.
 */
static esp_err_t init_audio_codec_es8388(uint32_t sample_rate_hz)
{
    ESP_LOGI(TAG,
             "TODO: init codec ES8388 ici (ADF/trombik) - sample_rate=%lu",
             (unsigned long)sample_rate_hz);

    /* Exemple (pseudo-code, à adapter quand tu auras les composants ADF) :

    audio_hal_codec_config_t cfg = AUDIO_HAL_ES8388_DEFAULT();
    cfg.i2s_iface.samples = AUDIO_HAL_16BITS;
    cfg.i2s_iface.fmt = AUDIO_HAL_I2S_NORMAL;
    cfg.i2s_iface.mode = AUDIO_HAL_MODE_SLAVE;
    cfg.i2s_iface.sample_rate = sample_rate_hz;

    audio_board_handle_t board = audio_board_init();
    audio_hal_handle_t hal = audio_board_audio_hal(board);
    audio_hal_ctrl_codec(hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    */

    return ESP_OK;
}

/*------------------------------------------------------------------------
 *  Parser WAV (PCM 16 bits)
 *-----------------------------------------------------------------------*/

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)(p[0] |
                      (p[1] << 8) |
                      (p[2] << 16) |
                      (p[3] << 24));
}

static esp_err_t parse_wav_header(FILE *f, wav_info_t *out)
{
    uint8_t hdr[12];

    if (fread(hdr, 1, 12, f) != 12) {
        ESP_LOGE(TAG, "Failed to read WAV header (12 bytes)");
        return ESP_FAIL;
    }

    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Not a RIFF/WAVE file");
        return ESP_FAIL;
    }

    bool fmt_found = false;
    bool data_found = false;
    uint32_t data_offset = 0;
    uint32_t data_size = 0;
    uint16_t audio_format = 0;
    uint16_t num_channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;

    while (!fmt_found || !data_found) {
        uint8_t chunk_hdr[8];
        if (fread(chunk_hdr, 1, 8, f) != 8) {
            ESP_LOGE(TAG, "Unexpected EOF while reading chunks");
            return ESP_FAIL;
        }

        uint32_t chunk_size = read_le32(chunk_hdr + 4);
        long chunk_data_pos = ftell(f);

        if (memcmp(chunk_hdr, "fmt ", 4) == 0) {
            if (chunk_size < 16) {
                ESP_LOGE(TAG, "fmt chunk too small");
                return ESP_FAIL;
            }
            uint8_t fmt[16];
            if (fread(fmt, 1, 16, f) != 16) {
                ESP_LOGE(TAG, "Failed to read fmt chunk");
                return ESP_FAIL;
            }

            audio_format = read_le16(fmt + 0);
            num_channels = read_le16(fmt + 2);
            sample_rate = read_le32(fmt + 4);
            bits_per_sample = read_le16(fmt + 14);
            fmt_found = true;

            // sauter le reste du chunk si besoin
            uint32_t fmt_extra = chunk_size - 16;
            if (fmt_extra > 0) {
                fseek(f, fmt_extra, SEEK_CUR);
            }
        } else if (memcmp(chunk_hdr, "data", 4) == 0) {
            data_offset = (uint32_t)ftell(f);
            data_size = chunk_size;
            // on ne saute pas : on est au début des données
            data_found = true;
            // NB: on laissera le pointeur sur ce début de data
        } else {
            // Chunk qu’on ignore
            fseek(f, chunk_size, SEEK_CUR);
        }

        // Sécurité: protection contre boucle infinie si fichier corrompu
        if (feof(f)) {
            break;
        }

        // Si on a trouvé data, on sort sans relire un chunk_hdr
        if (data_found && fmt_found) {
            break;
        }
    }

    if (!fmt_found || !data_found) {
        ESP_LOGE(TAG, "Missing fmt or data chunk");
        return ESP_FAIL;
    }

    if (audio_format != 1) {
        ESP_LOGE(TAG, "Unsupported WAV format (not PCM): %u",
                 audio_format);
        return ESP_FAIL;
    }

    if (bits_per_sample != 16) {
        ESP_LOGE(TAG, "Unsupported bits_per_sample=%u (need 16)",
                 bits_per_sample);
        return ESP_FAIL;
    }

    if (num_channels != 1 && num_channels != 2) {
        ESP_LOGE(TAG, "Unsupported num_channels=%u", num_channels);
        return ESP_FAIL;
    }

    out->sample_rate = sample_rate;
    out->num_channels = num_channels;
    out->bits_per_sample = bits_per_sample;
    out->data_offset = data_offset;
    out->data_size = data_size;

    ESP_LOGI(TAG,
             "WAV: %u Hz, %u ch, %u bits, data offset=%u size=%u",
             sample_rate, num_channels, bits_per_sample,
             data_offset, data_size);

    // replacer le pointeur fichier au début des données
    fseek(f, data_offset, SEEK_SET);

    return ESP_OK;
}

/*------------------------------------------------------------------------
 *  Lecture & lecture audio d’un WAV via I2S
 *-----------------------------------------------------------------------*/

static esp_err_t play_wav_from_sd(const char *rel_path)
{
    char path[256];
    snprintf(path, sizeof(path), "/sdcard/%s", rel_path);

    ESP_LOGI(TAG, "Opening WAV file: %s", path);

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s (%s)", path, strerror(errno));
        return ESP_FAIL;
    }

    wav_info_t info;
    esp_err_t ret = parse_wav_header(f, &info);
    if (ret != ESP_OK) {
        fclose(f);
        return ret;
    }

    // Reconfigure I2S pour ce WAV
    ESP_ERROR_CHECK(i2s_set_clk(I2S_PORT,
                                info.sample_rate,
                                I2S_BITS_PER_SAMPLE_16BIT,
                                (info.num_channels == 1)
                                    ? I2S_CHANNEL_MONO
                                    : I2S_CHANNEL_STEREO));

    // Ré-init éventuelle du codec à ce sample rate
    ESP_ERROR_CHECK(init_audio_codec_es8388(info.sample_rate));

    uint8_t buf[1024];
    size_t total = 0;

    while (1) {
        size_t r = fread(buf, 1, sizeof(buf), f);
        if (r == 0) {
            if (feof(f)) {
                ESP_LOGI(TAG,
                         "End of file, total bytes read=%u",
                         (unsigned)total);
            } else {
                ESP_LOGE(TAG, "Read error (%s)", strerror(errno));
            }
            break;
        }

        total += r;

        size_t bytes_written = 0;
        esp_err_t wret = i2s_write(I2S_PORT, buf, r,
                                   &bytes_written,
                                   portMAX_DELAY);
        if (wret != ESP_OK) {
            ESP_LOGE(TAG, "i2s_write error: %s",
                     esp_err_to_name(wret));
            break;
        }
    }

    fclose(f);
    return ESP_OK;
}

/*------------------------------------------------------------------------
 *  Commandes série
 *-----------------------------------------------------------------------*/

static void handle_command(const char *cmd)
{
    ESP_LOGI(TAG, "Command: '%s'", cmd);

    for (size_t i = 0; i < s_command_table_len; ++i) {
        if (strcmp(cmd, s_command_table[i].cmd) == 0) {
            ESP_LOGI(TAG, "-> play '%s'",
                     s_command_table[i].filename);
            esp_err_t ret =
                play_wav_from_sd(s_command_table[i].filename);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "play_wav_from_sd failed (%s)",
                         esp_err_to_name(ret));
            }
            return;
        }
    }

    ESP_LOGW(TAG, "Unknown command: '%s'", cmd);
}

static void serial_command_task(void *arg)
{
    char line[128];

    while (1) {
        printf("\nEnter command: ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        trim_newline(line);
        if (line[0] == '\0') {
            continue;
        }

        handle_command(line);
    }
}

/*------------------------------------------------------------------------
 *  app_main
 *-----------------------------------------------------------------------*/

void app_main(void)
{
    ESP_ERROR_CHECK(init_nvs());
    ESP_LOGI(TAG, "NVS OK");

    ESP_ERROR_CHECK(init_sdcard());
    ESP_ERROR_CHECK(init_i2c());

    // I2S initialisé avec un sample rate par défaut, sera ajusté
    // par play_wav_from_sd selon le fichier.
    ESP_ERROR_CHECK(init_i2s_default(44100));

    // Init de base codec (volume, routing, etc.) à 44.1kHz
    ESP_ERROR_CHECK(init_audio_codec_es8388(44100));

    xTaskCreatePinnedToCore(serial_command_task,
                            "serial_cmd",
                            4096,
                            NULL,
                            5,
                            NULL,
                            tskNO_AFFINITY);

    ESP_LOGI(TAG,
             "Setup complete, type a command (1, 2, HELLO, PING...)");
}
