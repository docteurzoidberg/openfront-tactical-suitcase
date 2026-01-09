/**
 * @file audio_decoder.c
 * @brief WAV file decoder task implementation
 * 
 * Handles decoding of WAV audio from both file and memory sources.
 * Converts 8-bit to 16-bit and resamples to 44.1kHz as needed.
 */

#include "audio_decoder.h"
#include "wav_utils.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "DECODER";

// Buffer sizes for conversion pipeline
#define CHUNK_SAMPLES       512   // Process this many samples at a time
#define CONVERT_BUF_SIZE    1024  // Large enough for resampling expansion
#define RESAMPLE_BUF_SIZE   1536  // ~3x for max expansion ratio

/**
 * @brief Convert and resample a chunk of audio data
 * 
 * Takes raw input data (8-bit or 16-bit) and outputs 16-bit 44.1kHz data.
 * 
 * @param input Raw input data
 * @param input_bytes Number of input bytes
 * @param wav_info Format information (sample rate, bit depth, channels)
 * @param convert_buf Working buffer for 8→16 bit conversion
 * @param resample_buf Working buffer for resampling
 * @param output Pointer to output buffer (set by function)
 * @param output_bytes Number of output bytes (set by function)
 */
static void convert_audio_chunk(
    const void *input,
    size_t input_bytes,
    const wav_info_t *wav_info,
    int16_t *convert_buf,
    int16_t *resample_buf,
    int16_t **output,
    size_t *output_bytes)
{
    int16_t *current_buf;
    size_t current_samples;
    
    // Step 1: Handle bit depth (8-bit → 16-bit conversion)
    if (wav_info->bits_per_sample == 8) {
        // 8-bit: each byte is one sample
        size_t num_samples = input_bytes;
        wav_convert_8bit_to_16bit((const uint8_t *)input, convert_buf, num_samples);
        current_buf = convert_buf;
        current_samples = num_samples;
    } else {
        // 16-bit: copy to working buffer
        size_t num_samples = input_bytes / sizeof(int16_t);
        memcpy(convert_buf, input, input_bytes);
        current_buf = convert_buf;
        current_samples = num_samples;
    }
    
    // Step 2: Resample if needed (any rate → 44.1kHz)
    if (wav_info->sample_rate != 44100) {
        size_t in_frames = current_samples / wav_info->num_channels;
        size_t out_frames = (in_frames * 44100) / wav_info->sample_rate;
        
        // Limit output to buffer size
        size_t max_out_frames = RESAMPLE_BUF_SIZE / wav_info->num_channels;
        if (out_frames > max_out_frames) {
            out_frames = max_out_frames;
        }
        
        size_t frames_written = wav_resample_linear(
            current_buf, in_frames, wav_info->sample_rate,
            resample_buf, out_frames, 44100, wav_info->num_channels);
        
        *output = resample_buf;
        *output_bytes = frames_written * wav_info->num_channels * sizeof(int16_t);
    } else {
        // No resampling needed
        *output = current_buf;
        *output_bytes = current_samples * sizeof(int16_t);
    }
}

/**
 * @brief Decode audio from memory buffer
 */
static void decode_memory_source(decoder_params_t *params) {
    const uint8_t *data = params->memory_data;
    size_t total_size = params->memory_size;
    size_t offset = 0;
    size_t total_output_bytes = 0;
    
    wav_info_t *wav = params->wav_info;
    size_t bytes_per_sample = (wav->bits_per_sample == 8) ? 1 : 2;
    
    // Working buffers
    int16_t convert_buf[CONVERT_BUF_SIZE];
    int16_t resample_buf[RESAMPLE_BUF_SIZE];
    
    while (offset < total_size && !(*params->stopping)) {
        // Calculate chunk size
        size_t remaining = total_size - offset;
        size_t chunk_bytes = CHUNK_SAMPLES * bytes_per_sample;
        if (chunk_bytes > remaining) {
            chunk_bytes = remaining;
        }
        
        // Convert and resample
        int16_t *output;
        size_t output_bytes;
        convert_audio_chunk(data + offset, chunk_bytes, wav,
                           convert_buf, resample_buf, &output, &output_bytes);
        
        // Send to stream buffer
        if (output_bytes > 0) {
            xStreamBufferSend(params->buffer, output, output_bytes, portMAX_DELAY);
            total_output_bytes += output_bytes;
        }
        
        offset += chunk_bytes;
    }
    
    *params->eof_reached = true;
    ESP_LOGD(TAG, "Memory source %d: %zu bytes in, %zu bytes out", 
             params->slot, total_size, total_output_bytes);
}

/**
 * @brief Decode audio from file
 */
static void decode_file_source(decoder_params_t *params) {
    FILE *fp = fopen(params->filepath, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open %s", params->filepath);
        return;
    }
    
    // Parse WAV header
    if (wav_parse_header(fp, params->wav_info) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid WAV file: %s", params->filepath);
        fclose(fp);
        return;
    }
    
    wav_info_t *wav = params->wav_info;
    ESP_LOGI(TAG, "Source %d: %luHz %dch %dbit", 
             params->slot, 
             (unsigned long)wav->sample_rate, 
             wav->num_channels, 
             wav->bits_per_sample);
    
    // Working buffers
    uint8_t read_buf[CHUNK_SAMPLES * 2];  // Max 2 bytes per sample
    int16_t convert_buf[CONVERT_BUF_SIZE];
    int16_t resample_buf[RESAMPLE_BUF_SIZE];
    
    size_t bytes_per_sample = wav->bits_per_sample / 8;
    size_t chunk_bytes = CHUNK_SAMPLES * wav->num_channels * bytes_per_sample;
    
    while (!(*params->stopping)) {
        size_t bytes_read = fread(read_buf, 1, chunk_bytes, fp);
        
        if (bytes_read == 0) {
            if (params->loop) {
                fseek(fp, wav->data_offset, SEEK_SET);
                continue;
            }
            *params->eof_reached = true;
            break;
        }
        
        // Convert and resample
        int16_t *output;
        size_t output_bytes;
        convert_audio_chunk(read_buf, bytes_read, wav,
                           convert_buf, resample_buf, &output, &output_bytes);
        
        // Send to stream buffer
        if (output_bytes > 0) {
            xStreamBufferSend(params->buffer, output, output_bytes, portMAX_DELAY);
        }
    }
    
    fclose(fp);
}

/**
 * @brief Decoder task entry point
 */
void audio_decoder_task(void *arg) {
    decoder_params_t *params = (decoder_params_t *)arg;
    
    ESP_LOGI(TAG, "Decoder task started for source %d: %s", 
             params->slot, params->filepath);
    
    if (params->memory_data != NULL) {
        decode_memory_source(params);
    } else {
        decode_file_source(params);
    }
    
    ESP_LOGI(TAG, "Decoder task stopped for source %d", params->slot);
    vTaskDelete(NULL);
}
