# CAN Bus Protocol - Developer Guide

**Last Updated**: January 5, 2026  
**Protocol Version**: 1.0 (Audio Module + Discovery)

## Purpose

This guide provides implementation patterns, code examples, and debugging strategies for developing CAN bus features in the OTS firmware. For protocol message formats and specifications, see [`/prompts/CANBUS_MESSAGE_SPEC.md`](../../prompts/CANBUS_MESSAGE_SPEC.md).

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Discovery Protocol Implementation](#discovery-protocol-implementation)
3. [Audio Protocol Implementation](#audio-protocol-implementation)
4. [Common Patterns](#common-patterns)
5. [Testing Strategies](#testing-strategies)
6. [Debugging](#debugging)
7. [Performance Considerations](#performance-considerations)

---

## Getting Started

### Prerequisites

- ESP-IDF v5.0 or later
- ESP32-S3 or ESP32-A1S board
- CAN transceiver (SN65HVD230, MCP2551, or TJA1050) for physical testing
- 120Ω termination resistors (optional for development with mock mode)

### Component Architecture

```
┌─────────────────────────────────────────┐
│  Application Layer                      │  Your firmware code
│  - Sound module, game events, etc.     │
├─────────────────────────────────────────┤
│  Discovery Layer (optional)             │  can_discovery component
│  - Boot-time module detection          │
├─────────────────────────────────────────┤
│  Protocol Layer (application-specific)  │  can_audio_protocol component
│  - Message encoding/decoding           │
├─────────────────────────────────────────┤
│  Hardware Layer                         │  can_driver component
│  - TWAI peripheral, frame TX/RX        │
└─────────────────────────────────────────┘
```

### Adding CAN Support to Your Project

**1. Add components to CMakeLists.txt:**

```cmake
# In your project's main CMakeLists.txt
set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_SOURCE_DIR}/../ots-fw-shared/components"
)

# In your component CMakeLists.txt
idf_component_register(
    SRCS "main.c" "can_handler.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        driver
        can_driver          # Hardware layer
        can_discovery       # Discovery protocol (optional)
        can_audiomodule     # Audio protocol (optional)
)
```

**2. Initialize CAN driver:**

```c
#include "can_driver.h"

void app_main(void) {
    // Initialize CAN with defaults (auto-detect hardware)
    can_config_t config = {
        .tx_pin = GPIO_NUM_5,
        .rx_pin = GPIO_NUM_4,
        .bitrate = 500000,
        .use_mock = false,   // Auto-detect TWAI hardware
    };
    
    esp_err_t ret = can_driver_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CAN init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "CAN driver initialized");
    
    // Start CAN RX task
    xTaskCreate(can_rx_task, "can_rx", 4096, NULL, 5, NULL);
}
```

**3. GPIO Pin Configuration:**

Main controller (ESP32-S3):
- TX: GPIO 5 (configurable)
- RX: GPIO 4 (configurable)

Audio module (ESP32-A1S):
- TX: GPIO 17 (configurable)
- RX: GPIO 16 (configurable)

**Note**: Always verify GPIO pins don't conflict with other peripherals (I2C, SPI, etc.).

---

## Discovery Protocol Implementation

### Module Side (Responding to Discovery)

**Example: Audio module responds to MODULE_QUERY**

```c
#include "can_driver.h"
#include "can_discovery.h"

void can_rx_task(void *pvParameters) {
    can_frame_t frame;
    
    while (1) {
        // Wait for CAN frame (blocking)
        esp_err_t ret = can_driver_receive(&frame, portMAX_DELAY);
        if (ret != ESP_OK) {
            continue;
        }
        
        // Handle discovery query
        if (frame.id == CAN_ID_MODULE_QUERY) {
            ESP_LOGI(TAG, "Discovery query received");
            
            // Auto-respond with MODULE_ANNOUNCE
            can_discovery_handle_query(&frame,
                MODULE_TYPE_AUDIO,      // Module type: Audio
                1,                      // Firmware major: 1
                0,                      // Firmware minor: 0
                MODULE_CAP_STATUS,      // Capabilities: STATUS
                0x42,                   // CAN block: 0x420-0x42F
                0                       // Node ID: 0 (primary)
            );
            
            ESP_LOGI(TAG, "Sent MODULE_ANNOUNCE");
        }
        
        // Handle other CAN messages (sound commands, etc.)
        else if (frame.id == CAN_ID_PLAY_SOUND) {
            handle_play_sound(&frame);
        }
        // ... more handlers ...
    }
}
```

**Manual announce (alternative):**

```c
void announce_module(void) {
    can_frame_t frame;
    can_discovery_announce(
        MODULE_TYPE_AUDIO,
        1, 0,                      // Version 1.0
        MODULE_CAP_STATUS,
        0x42, 0,
        &frame
    );
    can_driver_send(&frame, 100);
}
```

### Main Controller Side (Discovering Modules)

**Example: Discover and track audio module**

```c
#include "can_driver.h"
#include "can_discovery.h"

// Module registry
typedef struct {
    bool discovered;
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t can_block;
} audio_module_info_t;

static audio_module_info_t s_audio_module = {0};

void can_rx_task(void *pvParameters) {
    can_frame_t frame;
    
    while (1) {
        esp_err_t ret = can_driver_receive(&frame, portMAX_DELAY);
        if (ret != ESP_OK) {
            continue;
        }
        
        // Parse MODULE_ANNOUNCE
        if (frame.id == CAN_ID_MODULE_ANNOUNCE) {
            module_info_t info;
            if (can_discovery_parse_announce(&frame, &info) == ESP_OK) {
                
                // Check if audio module
                if (info.module_type == MODULE_TYPE_AUDIO) {
                    s_audio_module.discovered = true;
                    s_audio_module.version_major = info.version_major;
                    s_audio_module.version_minor = info.version_minor;
                    s_audio_module.can_block = info.can_block_base;
                    
                    ESP_LOGI(TAG, "✓ Audio module v%d.%d detected (CAN block 0x%02X)",
                             info.version_major, info.version_minor, info.can_block_base);
                }
            }
        }
        
        // Handle other messages (ACKs, STATUS, etc.)
        // ...
    }
}

void discover_modules(void) {
    ESP_LOGI(TAG, "Discovering CAN modules...");
    
    // Send query broadcast
    can_discovery_query_all();
    
    // Wait for responses (500ms is standard)
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Check what was discovered
    if (s_audio_module.discovered) {
        ESP_LOGI(TAG, "✓ Audio module ready");
        // Enable sound features
    } else {
        ESP_LOGW(TAG, "✗ No audio module detected - sound features disabled");
        // Disable sound features gracefully
    }
}

void app_main(void) {
    // ... CAN init ...
    
    // Start RX task first
    xTaskCreate(can_rx_task, "can_rx", 4096, NULL, 5, NULL);
    
    // Discover modules
    discover_modules();
    
    // Continue with normal initialization...
}
```

**Graceful Degradation Pattern:**

```c
bool sound_module_is_available(void) {
    return s_audio_module.discovered;
}

void play_sound(uint8_t sound_index) {
    if (!sound_module_is_available()) {
        ESP_LOGW(TAG, "Sound module not available, skipping sound %d", sound_index);
        return;
    }
    
    // Send PLAY_SOUND command...
}
```

---

## Audio Protocol Implementation

### Sending Commands (Main Controller)

**Example 1: Play one-shot sound**

```c
#include "can_audio_protocol.h"

// State tracking
static uint16_t g_request_id = 0;

void play_victory_sound(void) {
    can_frame_t frame;
    uint16_t sound_index = 1;  // game_victory.wav
    uint8_t flags = 0;          // No loop, no interrupt
    uint8_t volume = 100;       // 100%
    uint16_t request_id = can_audio_allocate_request_id(&g_request_id);
    
    // Build PLAY_SOUND frame
    can_audio_build_play_sound(sound_index, flags, volume, request_id, &frame);
    
    // Send with 100ms timeout
    esp_err_t ret = can_driver_send(&frame, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send PLAY_SOUND: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Sent PLAY_SOUND: index=%d vol=%d req=%d", 
                 sound_index, volume, request_id);
    }
}
```

**Example 2: Play looping alert**

```c
void play_alert_loop(uint8_t sound_index) {
    can_frame_t frame;
    uint8_t flags = CAN_AUDIO_FLAG_LOOP;  // Enable loop
    uint8_t volume = 80;                  // 80%
    uint16_t request_id = can_audio_allocate_request_id(&g_request_id);
    
    can_audio_build_play_sound(sound_index, flags, volume, request_id, &frame);
    can_driver_send(&frame, 100);
    
    ESP_LOGI(TAG, "Started looping alert: sound=%d", sound_index);
}
```

**Example 3: Stop sound by queue ID**

```c
void stop_sound(uint8_t queue_id) {
    // Validate queue ID
    if (!can_audio_queue_id_is_valid(queue_id)) {
        ESP_LOGW(TAG, "Invalid queue ID: %d", queue_id);
        return;
    }
    
    can_frame_t frame;
    uint16_t request_id = can_audio_allocate_request_id(&g_request_id);
    
    can_audio_build_stop_sound(queue_id, 0, request_id, &frame);
    can_driver_send(&frame, 100);
    
    ESP_LOGI(TAG, "Sent STOP_SOUND: queue_id=%d", queue_id);
}
```

**Example 4: Stop all sounds (panic button)**

```c
void stop_all_sounds(void) {
    can_frame_t frame;
    can_audio_build_stop_all(&frame);
    can_driver_send(&frame, 100);
    
    ESP_LOGI(TAG, "Sent STOP_ALL");
}
```

### Handling Responses (Main Controller)

**Parsing PLAY_SOUND_ACK:**

```c
// Queue ID tracking
#define MAX_ACTIVE_SOUNDS 4

typedef struct {
    uint8_t queue_id;
    uint16_t sound_index;
    bool is_looping;
    bool active;
} sound_handle_t;

static sound_handle_t s_active_sounds[MAX_ACTIVE_SOUNDS] = {0};

void handle_play_sound_ack(const can_frame_t *frame) {
    // Parse ACK
    uint8_t ok = frame->data[0];
    uint16_t sound_index = frame->data[1] | (frame->data[2] << 8);
    uint8_t queue_id = frame->data[3];
    uint8_t error_code = frame->data[4];
    uint16_t request_id = frame->data[5] | (frame->data[6] << 8);
    
    if (ok && can_audio_queue_id_is_valid(queue_id)) {
        ESP_LOGI(TAG, "✓ Sound %d started: queue_id=%d req=%d", 
                 sound_index, queue_id, request_id);
        
        // Track queue ID for later stop
        for (int i = 0; i < MAX_ACTIVE_SOUNDS; i++) {
            if (!s_active_sounds[i].active) {
                s_active_sounds[i].queue_id = queue_id;
                s_active_sounds[i].sound_index = sound_index;
                s_active_sounds[i].is_looping = false; // Set based on flags used
                s_active_sounds[i].active = true;
                break;
            }
        }
    } else {
        // Handle error
        ESP_LOGE(TAG, "✗ Sound %d failed: error=%d req=%d", 
                 sound_index, error_code, request_id);
        
        // Retry logic based on error code
        if (error_code == CAN_AUDIO_ERR_MIXER_FULL) {
            ESP_LOGW(TAG, "Mixer full, will retry in 500ms");
            // Schedule retry...
        }
    }
}
```

**Parsing SOUND_FINISHED:**

```c
void handle_sound_finished(const can_frame_t *frame) {
    uint8_t queue_id = frame->data[0];
    uint16_t sound_index = frame->data[1] | (frame->data[2] << 8);
    uint8_t reason = frame->data[3];
    
    ESP_LOGI(TAG, "Sound %d finished: queue_id=%d reason=%d", 
             sound_index, queue_id, reason);
    
    // Remove from active sounds
    for (int i = 0; i < MAX_ACTIVE_SOUNDS; i++) {
        if (s_active_sounds[i].queue_id == queue_id) {
            s_active_sounds[i].active = false;
            break;
        }
    }
    
    // Handle reason codes
    if (reason == 0x02) {
        ESP_LOGW(TAG, "Sound playback error for queue_id=%d", queue_id);
    }
}
```

### Handling Commands (Audio Module)

**Processing PLAY_SOUND:**

```c
#include "can_audio_protocol.h"
#include "audio_mixer.h"

static uint8_t g_queue_id_counter = 1;

void handle_play_sound(const can_frame_t *frame) {
    uint16_t sound_index;
    uint8_t flags, volume;
    uint16_t request_id;
    
    // Parse request
    if (!can_audio_parse_play_sound(frame, &sound_index, &flags, 
                                   &volume, &request_id)) {
        ESP_LOGE(TAG, "Failed to parse PLAY_SOUND");
        return;
    }
    
    ESP_LOGI(TAG, "PLAY_SOUND: idx=%d flags=0x%02X vol=%d req=%d",
             sound_index, flags, volume, request_id);
    
    // Check if looping
    bool loop = (flags & CAN_AUDIO_FLAG_LOOP) != 0;
    
    // Start audio playback
    esp_err_t ret = audio_mixer_play(sound_index, loop, volume);
    
    // Allocate queue ID
    uint8_t queue_id = 0;
    uint8_t error_code = CAN_AUDIO_ERR_OK;
    
    if (ret == ESP_OK) {
        queue_id = can_audio_allocate_queue_id(&g_queue_id_counter);
        ESP_LOGI(TAG, "Assigned queue_id=%d to sound %d", queue_id, sound_index);
    } else if (ret == ESP_ERR_NO_MEM) {
        error_code = CAN_AUDIO_ERR_MIXER_FULL;
        ESP_LOGW(TAG, "Mixer full, cannot play sound %d", sound_index);
    } else {
        error_code = CAN_AUDIO_ERR_SD_ERROR;
        ESP_LOGE(TAG, "Failed to play sound %d: %s", sound_index, esp_err_to_name(ret));
    }
    
    // Send ACK
    can_frame_t ack;
    can_audio_build_sound_ack(
        (ret == ESP_OK),  // ok flag
        sound_index,
        queue_id,
        error_code,
        request_id,
        &ack
    );
    
    can_driver_send(&ack, 50);  // Quick ACK (< 50ms target)
}
```

**Processing STOP_SOUND:**

```c
void handle_stop_sound(const can_frame_t *frame) {
    uint8_t queue_id;
    uint8_t flags;
    uint16_t request_id;
    
    if (!can_audio_parse_stop_sound(frame, &queue_id, &flags, &request_id)) {
        ESP_LOGE(TAG, "Failed to parse STOP_SOUND");
        return;
    }
    
    ESP_LOGI(TAG, "STOP_SOUND: queue_id=%d req=%d", queue_id, request_id);
    
    // Stop sound in mixer
    esp_err_t ret = audio_mixer_stop_by_queue_id(queue_id);
    
    // Send ACK
    can_frame_t ack;
    ack.id = CAN_ID_STOP_ACK;
    ack.dlc = 8;
    ack.data[0] = queue_id;
    ack.data[1] = (ret == ESP_OK) ? 0x00 : 0x01;  // Status
    memset(&ack.data[2], 0, 6);
    
    can_driver_send(&ack, 50);
}
```

**Sending SOUND_FINISHED notification:**

```c
void audio_mixer_on_sound_complete(uint8_t queue_id, uint16_t sound_index) {
    can_frame_t frame;
    can_audio_build_sound_finished(queue_id, sound_index, 0x00, &frame);
    can_driver_send(&frame, 50);
    
    ESP_LOGI(TAG, "Sent SOUND_FINISHED: queue_id=%d", queue_id);
}
```

---

## Common Patterns

### Pattern 1: Request-ACK-Timeout

**Main controller side:**

```c
#define ACK_TIMEOUT_MS 200

typedef struct {
    uint16_t request_id;
    uint64_t sent_time;
    bool ack_received;
    uint8_t queue_id;
} pending_request_t;

static pending_request_t s_pending = {0};

void send_play_sound_with_timeout(uint16_t sound_index) {
    // Send request
    can_frame_t frame;
    s_pending.request_id = can_audio_allocate_request_id(&g_request_id);
    s_pending.sent_time = esp_timer_get_time() / 1000;  // ms
    s_pending.ack_received = false;
    
    can_audio_build_play_sound(sound_index, 0, 100, s_pending.request_id, &frame);
    can_driver_send(&frame, 100);
    
    // Wait for ACK (blocking example)
    uint64_t start = s_pending.sent_time;
    while (!s_pending.ack_received) {
        uint64_t now = esp_timer_get_time() / 1000;
        if (now - start > ACK_TIMEOUT_MS) {
            ESP_LOGE(TAG, "ACK timeout for request %d", s_pending.request_id);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "ACK received: queue_id=%d", s_pending.queue_id);
}

void handle_ack(const can_frame_t *frame) {
    uint16_t request_id = frame->data[5] | (frame->data[6] << 8);
    
    if (request_id == s_pending.request_id) {
        s_pending.ack_received = true;
        s_pending.queue_id = frame->data[3];
    }
}
```

### Pattern 2: Retry on Mixer Full

```c
void play_sound_with_retry(uint16_t sound_index) {
    for (int retry = 0; retry < 2; retry++) {
        // Send play request
        can_frame_t frame;
        uint16_t req_id = can_audio_allocate_request_id(&g_request_id);
        can_audio_build_play_sound(sound_index, 0, 100, req_id, &frame);
        can_driver_send(&frame, 100);
        
        // Wait for ACK
        uint8_t error_code = wait_for_ack(req_id, 200);
        
        if (error_code == CAN_AUDIO_ERR_OK) {
            ESP_LOGI(TAG, "Sound %d started successfully", sound_index);
            return;
        } else if (error_code == CAN_AUDIO_ERR_MIXER_FULL && retry == 0) {
            ESP_LOGW(TAG, "Mixer full, retrying in 500ms...");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        } else {
            ESP_LOGE(TAG, "Failed to play sound %d: error=%d", sound_index, error_code);
            return;
        }
    }
}
```

### Pattern 3: Stop All on Game Event

```c
void on_game_end(void) {
    ESP_LOGI(TAG, "Game ended, stopping all sounds");
    
    // Send STOP_ALL
    can_frame_t frame;
    can_audio_build_stop_all(&frame);
    can_driver_send(&frame, 100);
    
    // Clear local tracking
    for (int i = 0; i < MAX_ACTIVE_SOUNDS; i++) {
        s_active_sounds[i].active = false;
    }
}
```

### Pattern 4: Loop Alert Until Event

```c
static uint8_t s_alert_queue_id = 0;

void start_alert(uint8_t sound_index) {
    can_frame_t frame;
    uint8_t flags = CAN_AUDIO_FLAG_LOOP;
    uint16_t req_id = can_audio_allocate_request_id(&g_request_id);
    
    can_audio_build_play_sound(sound_index, flags, 80, req_id, &frame);
    can_driver_send(&frame, 100);
    
    // Wait for ACK and store queue ID
    // (simplified, in practice use callback or polling)
    s_alert_queue_id = wait_for_ack_queue_id(req_id);
    
    ESP_LOGI(TAG, "Alert started: queue_id=%d", s_alert_queue_id);
}

void stop_alert_on_event(void) {
    if (s_alert_queue_id == 0) {
        return;  // No alert active
    }
    
    can_frame_t frame;
    uint16_t req_id = can_audio_allocate_request_id(&g_request_id);
    can_audio_build_stop_sound(s_alert_queue_id, 0, req_id, &frame);
    can_driver_send(&frame, 100);
    
    ESP_LOGI(TAG, "Stopped alert: queue_id=%d", s_alert_queue_id);
    s_alert_queue_id = 0;
}
```

---

## Testing Strategies

### Unit Testing with Mock Mode

CAN driver auto-detects hardware and falls back to mock mode if no transceiver found. Use this for protocol testing:

```c
void test_play_sound_message_format(void) {
    can_frame_t frame;
    uint16_t sound_index = 5;
    uint8_t flags = CAN_AUDIO_FLAG_LOOP;
    uint8_t volume = 80;
    uint16_t request_id = 123;
    
    can_audio_build_play_sound(sound_index, flags, volume, request_id, &frame);
    
    // Verify frame structure
    assert(frame.id == CAN_ID_PLAY_SOUND);
    assert(frame.dlc == 8);
    assert(frame.data[0] == 5);          // Sound index low
    assert(frame.data[1] == 0);          // Sound index high
    assert(frame.data[2] == flags);
    assert(frame.data[3] == volume);
    assert(frame.data[4] == 123);        // Request ID low
    assert(frame.data[5] == 0);          // Request ID high
    
    ESP_LOGI(TAG, "✓ PLAY_SOUND message format correct");
}
```

### Integration Testing

**Test 1: Discovery flow**

```bash
# Terminal 1: Audio module
cd ots-fw-audiomodule
pio run -e esp32-a1s-espidf -t upload && pio device monitor

# Terminal 2: Main controller
cd ots-fw-main
pio run -e esp32-s3-dev -t upload && pio device monitor

# Expected output (main controller):
# [can_discovery] Sent MODULE_QUERY
# [can_rx] MODULE_ANNOUNCE received: type=AUDIO v1.0
# [sound_module] ✓ Audio module detected
```

**Test 2: Play-ACK-Finished flow**

```c
void test_play_oneshot_sound(void) {
    ESP_LOGI(TAG, "=== Test: Play One-Shot Sound ===");
    
    // 1. Send PLAY_SOUND
    can_frame_t play_req;
    can_audio_build_play_sound(1, 0, 100, 1, &play_req);
    can_driver_send(&play_req, 100);
    ESP_LOGI(TAG, "1. Sent PLAY_SOUND (sound 1)");
    
    // 2. Wait for ACK (check in RX handler)
    vTaskDelay(pdMS_TO_TICKS(100));
    // Expected: ACK with queue_id assigned
    
    // 3. Wait for sound to finish
    vTaskDelay(pdMS_TO_TICKS(2000));
    // Expected: SOUND_FINISHED notification
    
    ESP_LOGI(TAG, "✓ Test complete");
}
```

**Test 3: Multiple simultaneous sounds**

```c
void test_multiple_sounds(void) {
    ESP_LOGI(TAG, "=== Test: Multiple Sounds ===");
    
    // Play 3 sounds quickly
    for (int i = 0; i < 3; i++) {
        can_frame_t frame;
        can_audio_build_play_sound(i, 0, 100, i+1, &frame);
        can_driver_send(&frame, 100);
        vTaskDelay(pdMS_TO_TICKS(50));  // Small delay between sends
    }
    
    // Wait for all to finish
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "✓ Multiple sounds test complete");
}
```

### Hardware Testing with CAN Analyzer

**Using ots-fw-cantest:**

```bash
cd ots-fw-cantest
pio run -e esp32-s3-devkit -t upload && pio device monitor

# Press 'm' for monitor mode
# Observe CAN traffic:
# [CAN] RX: ID=0x420 DLC=8 [01 00 00 64 01 00 00 00]  PLAY_SOUND
# [CAN] RX: ID=0x423 DLC=8 [01 01 00 03 00 01 00 00]  SOUND_ACK
```

**Expected traffic patterns:**

```
Time     ID    DLC  Data                              Description
------------------------------------------------------------------------
0.000    0x411  8   [FF 00 00 00 00 00 00 00]       MODULE_QUERY
0.035    0x410  8   [01 01 00 01 42 00 00 00]       MODULE_ANNOUNCE (audio)
5.000    0x420  8   [05 01 50 00 01 00 00 00]       PLAY_SOUND (sound 5, loop)
5.040    0x423  8   [01 05 00 07 00 01 00 00]       SOUND_ACK (queue_id=7)
15.000   0x421  8   [07 00 00 02 00 00 00 00]       STOP_SOUND (queue_id=7)
15.010   0x424  8   [07 00 00 00 00 00 00 00]       STOP_ACK
```

---

## Debugging

### Enable Detailed Logging

```c
// In your initialization code
esp_log_level_set("CAN_DRV", ESP_LOG_DEBUG);
esp_log_level_set("CAN_HANDLER", ESP_LOG_DEBUG);
esp_log_level_set("AUDIO_MIXER", ESP_LOG_DEBUG);
```

### Common Issues and Solutions

#### Issue 1: Mock Mode Always Active

**Symptom**: Logs show `[CAN_DRV] Initializing in MOCK mode` even with transceiver

**Causes**:
- Wrong GPIO pins configured
- Transceiver not powered (check VCC has 3.3V or 5V)
- TX/RX pins swapped
- Missing termination resistors

**Solution**:
```c
// Verify pin configuration
ESP_LOGI(TAG, "CAN TX pin: GPIO %d", config.tx_pin);
ESP_LOGI(TAG, "CAN RX pin: GPIO %d", config.rx_pin);

// Check if TWAI peripheral available
can_stats_t stats;
can_driver_get_stats(&stats);
ESP_LOGI(TAG, "CAN mode: %s", stats.is_mock ? "MOCK" : "PHYSICAL");
```

#### Issue 2: No ACK Received

**Symptom**: `can_driver_send()` succeeds but no ACK received

**Debug checklist**:
```c
// 1. Verify audio module is discovered
if (!s_audio_module.discovered) {
    ESP_LOGE(TAG, "Audio module not discovered!");
}

// 2. Check CAN bus activity
can_stats_t stats;
can_driver_get_stats(&stats);
ESP_LOGI(TAG, "TX: %lu, RX: %lu, Errors: %lu/%lu",
         stats.tx_count, stats.rx_count,
         stats.tx_errors, stats.rx_errors);

// 3. Verify message ID
ESP_LOGI(TAG, "Sent frame: ID=0x%03X DLC=%d", frame.id, frame.dlc);

// 4. Check audio module logs (serial monitor)
// Expected: "PLAY_SOUND received" and "Sent SOUND_ACK"
```

#### Issue 3: Queue ID Tracking Lost

**Symptom**: STOP_SOUND fails with "queue ID not found"

**Debug pattern**:
```c
void handle_ack(const can_frame_t *frame) {
    uint8_t queue_id = frame->data[3];
    
    ESP_LOGI(TAG, "ACK received: queue_id=%d", queue_id);
    
    // Log current tracking state
    for (int i = 0; i < MAX_ACTIVE_SOUNDS; i++) {
        if (s_active_sounds[i].active) {
            ESP_LOGI(TAG, "  Active[%d]: queue_id=%d sound=%d",
                     i, s_active_sounds[i].queue_id, s_active_sounds[i].sound_index);
        }
    }
}
```

#### Issue 4: Sounds Don't Play

**Audio module checklist**:

```c
// 1. Check SD card mounted
bool sd_mounted = audio_sd_card_is_mounted();
ESP_LOGI(TAG, "SD card mounted: %s", sd_mounted ? "YES" : "NO");

// 2. Verify sound file exists
const char *filepath = sound_config_get_path(sound_index);
ESP_LOGI(TAG, "Sound file: %s", filepath);

// 3. Check mixer status
uint8_t active_count = audio_mixer_get_active_count();
ESP_LOGI(TAG, "Active mixer sources: %d/4", active_count);

// 4. Test audio output directly
esp_err_t ret = audio_mixer_play(0, false, 100);
ESP_LOGI(TAG, "Direct play test: %s", esp_err_to_name(ret));
```

### CAN Bus Analyzer Tips

**Wireshark-style filtering:**
```
# Show only discovery messages
can.id == 0x410 || can.id == 0x411

# Show only audio protocol
can.id >= 0x420 && can.id <= 0x425

# Show errors only
can.error_flag == 1
```

**Common error patterns:**
- Bit errors: Check termination resistors (120Ω at each end)
- ACK errors: No other node on bus (connect second device)
- Stuff errors: Bitrate mismatch (verify 500 kbps on all nodes)

---

## Performance Considerations

### Bus Bandwidth

**CAN 2.0B at 500 kbps:**
- Theoretical max: ~7000 frames/second
- Practical max: ~2000 frames/second (with overhead)
- Audio protocol usage: < 100 frames/second (peak)
- **Bandwidth headroom**: > 95% available

### Latency Targets

| Operation | Target | Maximum | Notes |
|-----------|--------|---------|-------|
| ACK response | < 50ms | 200ms | Audio module processing + TX |
| Sound start | < 100ms | 500ms | From CAN RX to audio output |
| Stop command | < 10ms | 50ms | Immediate mixer stop |
| Discovery | N/A | 500ms | One-time at boot |

### Optimization Tips

**1. Minimize ACK delays:**
```c
// Process PLAY_SOUND immediately
void can_rx_task_high_priority(void *pvParameters) {
    // Set high priority for CAN RX
    vTaskPrioritySet(NULL, 6);  // Higher than normal tasks
    
    can_frame_t frame;
    while (1) {
        if (can_driver_receive(&frame, portMAX_DELAY) == ESP_OK) {
            // Handle in ISR context if possible (for fast ACK)
            handle_can_message(&frame);
        }
    }
}
```

**2. Batch non-critical operations:**
```c
// Send STATUS messages at lower frequency
#define STATUS_INTERVAL_MS 5000

void status_task(void *pvParameters) {
    while (1) {
        can_frame_t status;
        can_audio_build_sound_status(/* ... */, &status);
        can_driver_send(&status, 100);
        
        vTaskDelay(pdMS_TO_TICKS(STATUS_INTERVAL_MS));
    }
}
```

**3. Use non-blocking CAN operations:**
```c
// Don't block main loop waiting for ACK
esp_err_t ret = can_driver_send(&frame, 0);  // No timeout
if (ret == ESP_ERR_TIMEOUT) {
    // Queue for retry later
    retry_queue_push(&frame);
}
```

### Memory Usage

**Typical allocations:**
- CAN driver: ~1KB static + 2KB stack per task
- Message buffers: 8 bytes × queue depth (e.g., 20 × 8 = 160 bytes)
- Tracking arrays: 4 bytes × max concurrent sounds (e.g., 4 × 8 = 32 bytes)

**Total CAN overhead**: < 5KB RAM

---

## Reference Implementation

### Main Controller Template

See [`/ots-fw-main/src/sound_module.c`](../../ots-fw-main/src/sound_module.c) for complete implementation:
- Discovery integration
- PLAY_SOUND command sending
- ACK/FINISHED handling
- Queue ID tracking
- Error handling and retry

### Audio Module Template

See [`/ots-fw-audiomodule/src/can_audio_handler.c`](../../ots-fw-audiomodule/src/can_audio_handler.c) for complete implementation:
- MODULE_ANNOUNCE response
- PLAY_SOUND command processing
- Audio mixer integration
- Queue ID allocation
- ACK/FINISHED sending

### Testing Tool

See [`/ots-fw-cantest/`](../../ots-fw-cantest/) for CAN bus testing and debugging:
- Monitor mode (passive bus sniffer)
- Audio module simulator
- Controller simulator
- Interactive CLI

---

## Further Reading

**Protocol Specification**:
- [`/prompts/CANBUS_MESSAGE_SPEC.md`](../../prompts/CANBUS_MESSAGE_SPEC.md) - Complete message format reference

**Component Documentation**:
- [`/ots-fw-shared/components/can_driver/COMPONENT_PROMPT.md`](../../ots-fw-shared/components/can_driver/COMPONENT_PROMPT.md)
- [`/ots-fw-shared/components/can_discovery/COMPONENT_PROMPT.md`](../../ots-fw-shared/components/can_discovery/COMPONENT_PROMPT.md)
- [`/ots-fw-shared/components/can_audiomodule/COMPONENT_PROMPT.md`](../../ots-fw-shared/components/can_audiomodule/COMPONENT_PROMPT.md)

**ESP-IDF Resources**:
- [TWAI Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html)
- [CAN 2.0B Specification](https://www.bosch-semiconductors.com/media/ip_modules/pdf_2/can2spec.pdf)

---

**Questions or issues?** Check the [troubleshooting section](#debugging) or consult the component documentation.
