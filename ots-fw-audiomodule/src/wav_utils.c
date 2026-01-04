/**
 * @file wav_utils.c
 * @brief WAV File Utilities Implementation
 */

#include "wav_utils.h"
#include "esp_log.h"
#include <string.h>
#include <stdbool.h>

static const char *TAG = "WAV_UTILS";

uint16_t wav_read_le16(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

uint32_t wav_read_le32(const uint8_t *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

esp_err_t wav_parse_header(FILE *f, wav_info_t *info)
{
    if (!f || !info) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t chunk_hdr[8];
    
    // Read RIFF header
    if (fread(chunk_hdr, 1, 8, f) != 8) {
        ESP_LOGE(TAG, "Failed to read RIFF header");
        return ESP_ERR_INVALID_SIZE;
    }
    
    if (memcmp(chunk_hdr, "RIFF", 4) != 0) {
        ESP_LOGE(TAG, "Not a RIFF file");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Read WAVE format
    uint8_t wave[4];
    if (fread(wave, 1, 4, f) != 4 || memcmp(wave, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Not a WAVE file");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize defaults
    info->sample_rate = 0;
    info->num_channels = 0;
    info->bits_per_sample = 0;
    info->data_offset = 0;
    info->data_size = 0;
    
    bool found_fmt = false;
    bool found_data = false;
    
    // Parse chunks
    while (!found_data && fread(chunk_hdr, 1, 8, f) == 8) {
        uint32_t chunk_size = wav_read_le32(chunk_hdr + 4);
        
        if (memcmp(chunk_hdr, "fmt ", 4) == 0) {
            // Read format chunk
            uint8_t fmt[16];
            size_t to_read = chunk_size < 16 ? chunk_size : 16;
            if (fread(fmt, 1, to_read, f) != to_read) {
                ESP_LOGE(TAG, "Failed to read fmt chunk");
                return ESP_ERR_INVALID_SIZE;
            }
            
            uint16_t audio_format = wav_read_le16(fmt + 0);
            uint16_t num_channels = wav_read_le16(fmt + 2);
            uint32_t sample_rate = wav_read_le32(fmt + 4);
            uint16_t bits_per_sample = wav_read_le16(fmt + 14);
            
            if (audio_format != 1) {
                ESP_LOGE(TAG, "Only PCM format supported (got %d)", audio_format);
                return ESP_ERR_NOT_SUPPORTED;
            }
            
            info->num_channels = num_channels;
            info->sample_rate = sample_rate;
            info->bits_per_sample = bits_per_sample;
            found_fmt = true;
            
            // Skip remaining fmt data
            if (chunk_size > 16) {
                fseek(f, chunk_size - 16, SEEK_CUR);
            }
            
        } else if (memcmp(chunk_hdr, "data", 4) == 0) {
            // Found data chunk
            info->data_offset = ftell(f);
            info->data_size = chunk_size;
            found_data = true;
            
        } else {
            // Skip unknown chunk
            fseek(f, chunk_size, SEEK_CUR);
        }
    }
    
    if (!found_fmt || !found_data) {
        ESP_LOGE(TAG, "Missing required chunks (fmt=%d, data=%d)", found_fmt, found_data);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "WAV: %luHz, %dch, %dbit, %lu bytes",
             (unsigned long)info->sample_rate,
             info->num_channels,
             info->bits_per_sample,
             (unsigned long)info->data_size);
    
    return ESP_OK;
}

esp_err_t wav_parse_header_from_memory(const uint8_t *data, wav_info_t *info)
{
    if (!data || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    const uint8_t *p = data;
    
    // Check RIFF header
    if (memcmp(p, "RIFF", 4) != 0) {
        ESP_LOGE(TAG, "Not a RIFF file");
        return ESP_ERR_INVALID_ARG;
    }
    p += 8;  // Skip "RIFF" + size
    
    // Check WAVE format
    if (memcmp(p, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Not a WAVE file");
        return ESP_ERR_INVALID_ARG;
    }
    p += 4;
    
    // Initialize defaults
    info->sample_rate = 0;
    info->num_channels = 0;
    info->bits_per_sample = 0;
    info->data_offset = 0;
    info->data_size = 0;
    
    bool found_fmt = false;
    bool found_data = false;
    
    // Parse chunks
    while (!found_data) {
        // Check chunk header (4 bytes ID + 4 bytes size)
        if (p >= data + 1000) {  // Safety check
            ESP_LOGE(TAG, "Chunk parsing exceeded buffer");
            return ESP_ERR_INVALID_SIZE;
        }
        
        uint32_t chunk_size = wav_read_le32(p + 4);
        
        if (memcmp(p, "fmt ", 4) == 0) {
            // Parse format chunk
            const uint8_t *fmt = p + 8;
            
            uint16_t audio_format = wav_read_le16(fmt + 0);
            uint16_t num_channels = wav_read_le16(fmt + 2);
            uint32_t sample_rate = wav_read_le32(fmt + 4);
            uint16_t bits_per_sample = wav_read_le16(fmt + 14);
            
            if (audio_format != 1) {
                ESP_LOGE(TAG, "Only PCM format supported (got %d)", audio_format);
                return ESP_ERR_NOT_SUPPORTED;
            }
            
            info->num_channels = num_channels;
            info->sample_rate = sample_rate;
            info->bits_per_sample = bits_per_sample;
            found_fmt = true;
            
        } else if (memcmp(p, "data", 4) == 0) {
            // Found data chunk
            info->data_offset = (p + 8) - data;
            info->data_size = chunk_size;
            found_data = true;
        }
        
        // Move to next chunk
        p += 8 + chunk_size;
    }
    
    if (!found_fmt || !found_data) {
        ESP_LOGE(TAG, "Missing fmt or data chunk");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Parsed WAV from memory: %lu Hz, %d ch, %d bits, %lu bytes",
             info->sample_rate, info->num_channels, info->bits_per_sample, info->data_size);
    
    return ESP_OK;
}
