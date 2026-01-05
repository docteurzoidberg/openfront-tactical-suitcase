/**
 * @file audio_console.c
 * @brief Interactive console for audio control
 * 
 * Unified console interface with command handling and UI formatting
 */

#include "audio_console.h"
#include "audio_tone_player.h"
#include "audio_mixer.h"
#include "audio_player.h"
#include "hardware/sdcard.h"

#include "esp_log.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

static const char *TAG = "CONSOLE";

/*------------------------------------------------------------------------
 *  UI Formatting Helpers
 *-----------------------------------------------------------------------*/

static void print_banner(void)
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

static void print_mixer_status(void)
{
    int active = audio_mixer_get_active_count();
    uint8_t volume = audio_mixer_get_master_volume();
    
    ESP_LOGI(TAG, "═══ Mixer Status ═══");
    ESP_LOGI(TAG, "Active sources: %d / %d", active, MAX_AUDIO_SOURCES);
    ESP_LOGI(TAG, "Master volume:  %d%%", volume);
}

static void print_playing_sources(void)
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
}

static void print_sysinfo(void)
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
}

static void print_tone_info(void)
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
}

/*------------------------------------------------------------------------
 *  Console Command Handlers
 *-----------------------------------------------------------------------*/

// Play WAV file from SD card
static int cmd_play(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: play <filename>\n");
        printf("  Example: play track1.wav\n");
        return 1;
    }
    
    ESP_LOGI(TAG, "Playing WAV file: %s", argv[1]);
    esp_err_t ret = audio_player_play_wav(argv[1]);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Playing %s", argv[1]);
        return 0;
    } else {
        ESP_LOGE(TAG, "✗ Failed to play %s", argv[1]);
        return 1;
    }
}

// Quick play command table
typedef struct {
    const char *filename;
    const char *description;
} quick_play_entry_t;

static const quick_play_entry_t g_quick_play_map[] = {
    {"track1.wav", "sound 1"},
    {"track2.wav", "sound 2"},
};

// Helper function for quick-play commands
static int play_wav_file(const char *filename, const char *description)
{
    ESP_LOGI(TAG, "Playing %s (%s)", description, filename);
    esp_err_t ret = audio_player_play_wav(filename);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Playing %s", filename);
    } else {
        ESP_LOGE(TAG, "✗ Failed to play %s", filename);
    }
    return (ret == ESP_OK) ? 0 : 1;
}

// Quick play commands (shortcuts)
static int cmd_play1(int argc, char **argv) {
    return play_wav_file(g_quick_play_map[0].filename, g_quick_play_map[0].description);
}

static int cmd_play2(int argc, char **argv) {
    return play_wav_file(g_quick_play_map[1].filename, g_quick_play_map[1].description);
}

static int cmd_hello(int argc, char **argv) {
    return play_wav_file("hello.wav", "hello sound");
}

static int cmd_ping(int argc, char **argv) {
    return play_wav_file("ping.wav", "ping sound");
}

// Tone playback commands - now using tone_player module
static int cmd_tone1(int argc, char **argv) {
    return (tone_player_play(TONE_ID_1, 100) == ESP_OK) ? 0 : 1;
}

static int cmd_tone2(int argc, char **argv) {
    return (tone_player_play(TONE_ID_2, 100) == ESP_OK) ? 0 : 1;
}

static int cmd_tone3(int argc, char **argv) {
    return (tone_player_play(TONE_ID_3, 100) == ESP_OK) ? 0 : 1;
}

static int cmd_mix(int argc, char **argv)
{
    return (tone_player_mix_all() == ESP_OK) ? 0 : 1;
}

// Deprecated: old tone playback helper (kept for compatibility)
static int cmd_status(int argc, char **argv)
{
    print_mixer_status();
    return 0;
}

static int cmd_test_tone(int argc, char **argv)
{
    ESP_LOGI(TAG, "Playing test tone (tone 1)...");
    return tone_player_play(TONE_ID_1, 100);
}

// Show currently playing sources
static int cmd_playing(int argc, char **argv)
{
    int active_count = audio_mixer_get_active_count();
    
    if (active_count == 0) {
        printf("No audio currently playing\n");
        return 0;
    }
    
    print_playing_sources();
    return 0;
}

// Pause a source (pauses all for simplicity)
static int cmd_pause(int argc, char **argv)
{
    // For simplicity, pause all active sources
    int paused = 0;
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (audio_mixer_pause_source(i) == ESP_OK) {
            paused++;
        }
    }
    
    if (paused > 0) {
        ESP_LOGI(TAG, "✓ Paused %d source(s)", paused);
        return 0;
    } else {
        printf("No active sources to pause\n");
        return 1;
    }
}

// Resume paused sources
static int cmd_resume(int argc, char **argv)
{
    // Resume all paused sources
    int resumed = 0;
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (audio_mixer_resume_source(i) == ESP_OK) {
            resumed++;
        }
    }
    
    if (resumed > 0) {
        ESP_LOGI(TAG, "✓ Resumed %d source(s)", resumed);
        return 0;
    } else {
        printf("No paused sources to resume\n");
        return 1;
    }
}

static int cmd_volume(int argc, char **argv)
{
    if (argc < 2) {
        // Show current volume
        uint8_t vol = audio_mixer_get_master_volume();
        printf("Current master volume: %d%%\n", vol);
        return 0;
    }
    
    int vol = atoi(argv[1]);
    if (vol < 0 || vol > 100) {
        printf("Error: Volume must be 0-100\n");
        return 1;
    }
    
    audio_mixer_set_master_volume((uint8_t)vol);
    ESP_LOGI(TAG, "✓ Master volume set to %d%%", vol);
    return 0;
}

static int cmd_stop(int argc, char **argv)
{
    ESP_LOGI(TAG, "Stopping all audio...");
    audio_mixer_stop_all();
    ESP_LOGI(TAG, "✓ All audio stopped");
    return 0;
}

static int cmd_info(int argc, char **argv)
{
    print_tone_info();
    return 0;
}

// SD card file listing
static int cmd_ls(int argc, char **argv)
{
    if (!sdcard_is_mounted()) {
        printf("Error: SD card not mounted\n");
        return 1;
    }
    
    DIR *dir = opendir(SD_CARD_MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory");
        return 1;
    }
    
    ESP_LOGI(TAG, "═══ WAV Files on SD Card ═══");
    struct dirent *entry;
    int count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // Filter for WAV files
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len > 4 && strcasecmp(name + len - 4, ".wav") == 0) {
            printf("  %s\n", name);
            count++;
        }
    }
    
    closedir(dir);
    
    if (count == 0) {
        printf("  (no .wav files found)\n");
    } else {
        ESP_LOGI(TAG, "Found %d WAV file(s)", count);
    }
    
    return 0;
}

// System status information
static int cmd_sysinfo(int argc, char **argv)
{
    print_sysinfo();
    return 0;
}

/*------------------------------------------------------------------------
 *  Command Registration
 *-----------------------------------------------------------------------*/

static void register_commands(void)
{
    const esp_console_cmd_t cmds[] = {
        // SD card WAV playback
        {
            .command = "play",
            .help = "Play WAV file from SD card\n  Usage: play <filename>",
            .hint = "<filename>",
            .func = &cmd_play,
        },
        {
            .command = "1",
            .help = "Play sound 1 (track1.wav)",
            .hint = NULL,
            .func = &cmd_play1,
        },
        {
            .command = "2",
            .help = "Play sound 2 (track2.wav)",
            .hint = NULL,
            .func = &cmd_play2,
        },
        {
            .command = "hello",
            .help = "Play hello sound (hello.wav)",
            .hint = NULL,
            .func = &cmd_hello,
        },
        {
            .command = "ping",
            .help = "Play ping sound (ping.wav)",
            .hint = NULL,
            .func = &cmd_ping,
        },
        // Embedded test tones
        {
            .command = "tone1",
            .help = "Play tone 1 (1s, 440Hz)",
            .hint = NULL,
            .func = &cmd_tone1,
        },
        {
            .command = "tone2",
            .help = "Play tone 2 (2s, 880Hz)",
            .hint = NULL,
            .func = &cmd_tone2,
        },
        {
            .command = "tone3",
            .help = "Play tone 3 (5s, 220Hz)",
            .hint = NULL,
            .func = &cmd_tone3,
        },
        {
            .command = "mix",
            .help = "Mix all tones simultaneously",
            .hint = NULL,
            .func = &cmd_mix,
        },
        {
            .command = "status",
            .help = "Show mixer status",
            .hint = NULL,
            .func = &cmd_status,
        },
        {
            .command = "test",
            .help = "Play built-in test tone",
            .hint = NULL,
            .func = &cmd_test_tone,
        },
        {
            .command = "volume",
            .help = "Get/set master volume (0-100)",
            .hint = "[0-100]",
            .func = &cmd_volume,
        },
        {
            .command = "stop",
            .help = "Stop all audio",
            .hint = NULL,
            .func = &cmd_stop,
        },
        {
            .command = "playing",
            .help = "Show currently playing sources",
            .hint = NULL,
            .func = &cmd_playing,
        },
        {
            .command = "pause",
            .help = "Pause playback",
            .hint = NULL,
            .func = &cmd_pause,
        },
        {
            .command = "resume",
            .help = "Resume playback",
            .hint = NULL,
            .func = &cmd_resume,
        },
        {
            .command = "info",
            .help = "Show embedded tone information",
            .hint = NULL,
            .func = &cmd_info,
        },
        // System commands
        {
            .command = "ls",
            .help = "List WAV files on SD card",
            .hint = NULL,
            .func = &cmd_ls,
        },
        {
            .command = "sysinfo",
            .help = "Show system information",
            .hint = NULL,
            .func = &cmd_sysinfo,
        },
    };
    
    for (int i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}

/*------------------------------------------------------------------------
 *  Public API
 *-----------------------------------------------------------------------*/

esp_err_t audio_console_init(void)
{
    print_banner();
    
    // Register commands
    register_commands();
    
    return ESP_OK;
}

esp_err_t audio_console_start(void)
{
    // Start the console REPL on UART
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "audio> ";
    repl_config.max_cmdline_length = 256;
    
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_err_t ret = esp_console_new_repl_uart(&uart_config, &repl_config, &repl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create console REPL: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_console_start_repl(repl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start console REPL: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Console REPL started - ready for commands");
    return ESP_OK;
}

esp_err_t audio_console_stop(void)
{
    // Console REPL doesn't support stopping once started
    ESP_LOGW(TAG, "Console REPL cannot be stopped after starting");
    return ESP_ERR_NOT_SUPPORTED;
}
