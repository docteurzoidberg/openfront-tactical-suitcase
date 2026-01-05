#include "nuke_state_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "OTS_NUKE_TRK";

#define MAX_TRACKED_NUKES 32  // Maximum simultaneous nukes to track

typedef struct {
    uint32_t unit_id;
    nuke_type_t type;
    nuke_direction_t direction;
    nuke_state_t state;
    bool active;
} tracked_nuke_t;

static tracked_nuke_t tracked_nukes[MAX_TRACKED_NUKES];
static bool initialized = false;

esp_err_t nuke_tracker_init(void) {
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    memset(tracked_nukes, 0, sizeof(tracked_nukes));
    initialized = true;
    
    ESP_LOGI(TAG, "Nuke tracker initialized (max %d nukes)", MAX_TRACKED_NUKES);
    return ESP_OK;
}

esp_err_t nuke_tracker_register_launch(uint32_t unit_id, nuke_type_t type, nuke_direction_t direction) {
    if (!initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (type >= NUKE_TYPE_COUNT) {
        ESP_LOGE(TAG, "Invalid nuke type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if already tracked
    for (int i = 0; i < MAX_TRACKED_NUKES; i++) {
        if (tracked_nukes[i].active && tracked_nukes[i].unit_id == unit_id) {
            ESP_LOGW(TAG, "Nuke %lu already tracked", (unsigned long)unit_id);
            return ESP_OK;
        }
    }
    
    // Find empty slot
    for (int i = 0; i < MAX_TRACKED_NUKES; i++) {
        if (!tracked_nukes[i].active) {
            tracked_nukes[i].unit_id = unit_id;
            tracked_nukes[i].type = type;
            tracked_nukes[i].direction = direction;
            tracked_nukes[i].state = NUKE_STATE_IN_FLIGHT;
            tracked_nukes[i].active = true;
            
            ESP_LOGI(TAG, "Registered nuke %lu: type=%d dir=%s", 
                    (unsigned long)unit_id, type, 
                    direction == NUKE_DIR_INCOMING ? "IN" : "OUT");
            
            return ESP_OK;
        }
    }
    
    ESP_LOGE(TAG, "No free slots for nuke tracking (max %d)", MAX_TRACKED_NUKES);
    return ESP_ERR_NO_MEM;
}

esp_err_t nuke_tracker_resolve_nuke(uint32_t unit_id, bool exploded) {
    if (!initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    for (int i = 0; i < MAX_TRACKED_NUKES; i++) {
        if (tracked_nukes[i].active && tracked_nukes[i].unit_id == unit_id) {
            tracked_nukes[i].state = exploded ? NUKE_STATE_EXPLODED : NUKE_STATE_INTERCEPTED;
            
            ESP_LOGI(TAG, "Resolved nuke %lu: type=%d dir=%s state=%s", 
                    (unsigned long)unit_id, tracked_nukes[i].type,
                    tracked_nukes[i].direction == NUKE_DIR_INCOMING ? "IN" : "OUT",
                    exploded ? "EXPLODED" : "INTERCEPTED");
            
            // Deactivate the slot
            tracked_nukes[i].active = false;
            
            return ESP_OK;
        }
    }
    
    ESP_LOGW(TAG, "Nuke %lu not found in tracker", (unsigned long)unit_id);
    return ESP_ERR_NOT_FOUND;
}

uint8_t nuke_tracker_get_active_count(nuke_type_t type, nuke_direction_t direction) {
    if (!initialized || type >= NUKE_TYPE_COUNT) {
        return 0;
    }
    
    uint8_t count = 0;
    for (int i = 0; i < MAX_TRACKED_NUKES; i++) {
        if (tracked_nukes[i].active && 
            tracked_nukes[i].type == type && 
            tracked_nukes[i].direction == direction &&
            tracked_nukes[i].state == NUKE_STATE_IN_FLIGHT) {
            count++;
        }
    }
    
    return count;
}

void nuke_tracker_clear_all(void) {
    if (!initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Clearing all tracked nukes");
    memset(tracked_nukes, 0, sizeof(tracked_nukes));
}

void nuke_tracker_get_stats(nuke_type_t type, nuke_direction_t direction,
                           uint8_t *out_in_flight, uint8_t *out_exploded, 
                           uint8_t *out_intercepted) {
    if (!initialized || type >= NUKE_TYPE_COUNT) {
        if (out_in_flight) *out_in_flight = 0;
        if (out_exploded) *out_exploded = 0;
        if (out_intercepted) *out_intercepted = 0;
        return;
    }
    
    uint8_t in_flight = 0, exploded = 0, intercepted = 0;
    
    for (int i = 0; i < MAX_TRACKED_NUKES; i++) {
        if (tracked_nukes[i].active && 
            tracked_nukes[i].type == type && 
            tracked_nukes[i].direction == direction) {
            switch (tracked_nukes[i].state) {
                case NUKE_STATE_IN_FLIGHT:
                    in_flight++;
                    break;
                case NUKE_STATE_EXPLODED:
                    exploded++;
                    break;
                case NUKE_STATE_INTERCEPTED:
                    intercepted++;
                    break;
            }
        }
    }
    
    if (out_in_flight) *out_in_flight = in_flight;
    if (out_exploded) *out_exploded = exploded;
    if (out_intercepted) *out_intercepted = intercepted;
}
