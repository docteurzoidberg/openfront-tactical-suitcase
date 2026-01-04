/**
 * @file audio_decoder.c
 * @brief WAV file decoder task implementation
 */

#include "audio_decoder.h"
#include "wav_utils.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "DECODER";

void audio_decoder_task(void *arg) {
    decoder_params_t *params = (decoder_params_t *)arg;
    
    ESP_LOGI(TAG, "Decoder task started for source %d: %s", params->slot, params->filepath);
    
    // Check if this is a memory source
    if (params->memory_data != NULL) {
        // Memory source - stream from memory buffer
        const uint8_t *data = params->memory_data;
        size_t total_size = params->memory_size;
        size_t offset = 0;
        const size_t chunk_size = 1024;  // Stream in 1KB chunks
        
        while (offset < total_size) {
            // Check if we should stop
            if (*params->stopping) {
                break;
            }
            
            // Calculate how much to send
            size_t remaining = total_size - offset;
            size_t to_send = (remaining < chunk_size) ? remaining : chunk_size;
            
            // Send chunk to buffer (blocking if buffer full)
            xStreamBufferSend(params->buffer, data + offset, to_send, portMAX_DELAY);
            offset += to_send;
        }
        
        // Mark EOF
        *params->eof_reached = true;
        ESP_LOGI(TAG, "Memory source %d complete: %zu bytes streamed", params->slot, total_size);
        vTaskDelete(NULL);
        return;
    }
    
    // File source - original file reading logic
    FILE *fp = NULL;
    int16_t read_buffer[512];
    
    while (true) {
        // Check if we should stop
        if (*params->stopping) {
            break;
        }
        
        // Open file if not already open
        if (fp == NULL) {
            fp = fopen(params->filepath, "rb");
            if (fp == NULL) {
                ESP_LOGE(TAG, "Failed to open %s (SD card not mounted?)", params->filepath);
                break;
            }
            
            // Parse WAV header
            if (wav_parse_header(fp, params->wav_info) != ESP_OK) {
                ESP_LOGE(TAG, "Invalid WAV file: %s", params->filepath);
                fclose(fp);
                break;
            }
            
            ESP_LOGI(TAG, "Source %d: %luHz %dch %dbit", 
                     params->slot, 
                     (unsigned long)params->wav_info->sample_rate, 
                     params->wav_info->num_channels, 
                     params->wav_info->bits_per_sample);
        }
        
        // Read PCM data
        size_t samples_to_read = sizeof(read_buffer) / (params->wav_info->num_channels * 2);
        size_t bytes_read = fread(read_buffer, 1, 
                                  samples_to_read * params->wav_info->num_channels * 2, fp);
        
        if (bytes_read == 0) {
            // EOF reached
            if (params->loop) {
                // Loop back to beginning
                fseek(fp, params->wav_info->data_offset, SEEK_SET);
                continue;
            } else {
                // End of playback
                *params->eof_reached = true;
                break;
            }
        }
        
        // Write to stream buffer (blocking if buffer full)
        xStreamBufferSend(params->buffer, read_buffer, bytes_read, portMAX_DELAY);
    }
    
    // Cleanup
    if (fp != NULL) {
        fclose(fp);
    }
    
    ESP_LOGI(TAG, "Decoder task stopped for source %d", params->slot);
    
    vTaskDelete(NULL);
}
