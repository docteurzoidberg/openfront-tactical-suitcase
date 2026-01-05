/**
 * @file audio_console_ui.c
 * @brief Console UI formatting and display utilities
 */

#include "audio_console_ui.h"
#include "audio_mixer.h"
#include "audio_tone_player.h"
#include "hardware/sdcard.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_system.h"
#include <stdio.h>

static const char *TAG = "CONSOLE_UI";

void console_ui_print_banner(void)
{
    ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║    AUDIO CONSOLE - Interactive Menu    ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "SD Card Commands:");
    ESP_LOGI(TAG, "  play <file>  - Play WAV from SD card");
    ESP_LOGI(TAG, "  1, 2         - Quick play track1/2.wav");
    ESP_LOGI(TAG, "  hello, ping  - Play hello/ping.wav");
    ESP_LOGI(TAG, "");
    
    // Get tone info dynamically
    size_t size;
    const char *desc;
    ESP_LOGI(TAG, "Embedded Test Tones:");
    for (tone_id_t id = TONE_ID_1; id < TONE_ID_MAX; id++) {
        if (tone_player_get_info(id, &size, &desc) == ESP_OK) {
            ESP_LOGI(TAG, "  • Tone %d: %zu bytes (%s)", id + 1, size, desc);
        }
    }
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Type 'help' to see all available commands");
}

esp_err_t console_ui_print_status(void)
{
    int active = audio_mixer_get_active_count();
    uint8_t volume = audio_mixer_get_master_volume();
    
    ESP_LOGI(TAG, "═══ Mixer Status ═══");
    ESP_LOGI(TAG, "Active sources: %d / %d", active, MAX_AUDIO_SOURCES);
    ESP_LOGI(TAG, "Master volume:  %d%%", volume);
    
    return ESP_OK;
}

esp_err_t console_ui_print_playing(void)
{
    ESP_LOGI(TAG, "═══ Currently Playing ═══");
    
    int found = 0;
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        char filepath[128];
        uint8_t volume;
        int state;
        
        if (audio_mixer_get_source_info(i, filepath, sizeof(filepath), &volume, &state) == ESP_OK) {
            const char *state_str = (state == 1) ? "PLAYING" : 
                                   (state == 2) ? "PAUSED" : "UNKNOWN";
            ESP_LOGI(TAG, "  [%d] %s (vol: %d%%, state: %s)", i, filepath, volume, state_str);
            found++;
        }
    }
    
    if (found == 0) {
        printf("  No active sources\n");
    }
    
    return ESP_OK;
}

esp_err_t console_ui_print_sysinfo(void)
{
    ESP_LOGI(TAG, "═══ System Information ═══");
    
    // Memory info
    printf("Memory:\n");
    printf("  Heap free: %lu bytes\n", (unsigned long)esp_get_free_heap_size());
    printf("  Heap min:  %lu bytes\n", (unsigned long)esp_get_minimum_free_heap_size());
    
    // PSRAM info
    size_t psram_total = esp_psram_get_size();
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (psram_total > 0) {
        printf("  PSRAM total: %zu bytes\n", psram_total);
        printf("  PSRAM free:  %zu bytes\n", psram_free);
    }
    
    printf("\n");
    
    // SD card status
    printf("SD Card:\n");
    if (sdcard_is_mounted()) {
        printf("  Status: Mounted\n");
    } else {
        printf("  Status: Not mounted\n");
    }
    
    printf("\n");
    
    // Audio mixer status
    printf("Audio Mixer:\n");
    printf("  Active sources: %d / %d\n", audio_mixer_get_active_count(), MAX_AUDIO_SOURCES);
    printf("  Master volume:  %d%%\n", audio_mixer_get_master_volume());
    
    // PSRAM utilization
    if (psram_total > 0) {
        size_t psram_used = psram_total - psram_free;
        float utilization = (float)psram_used / psram_total * 100.0f;
        
        printf("\nPSRAM Utilization:\n");
        printf("  Usage: %.1f%% (%zu / %zu bytes)\n", utilization, psram_used, psram_total);
        printf("  Audio buffers: Mixer + %d source streams\n", audio_mixer_get_active_count());
    }
    
    return ESP_OK;
}

esp_err_t console_ui_print_tone_info(void)
{
    ESP_LOGI(TAG, "═══ Embedded Test Tones ═══");
    
    for (tone_id_t id = TONE_ID_1; id < TONE_ID_MAX; id++) {
        size_t size;
        const char *desc;
        if (tone_player_get_info(id, &size, &desc) == ESP_OK) {
            ESP_LOGI(TAG, "Tone %d: %zu bytes (%s)", id + 1, size, desc);
        }
    }
    
    ESP_LOGI(TAG, "Total: %zu bytes", tone_player_get_total_size());
    return ESP_OK;
}
