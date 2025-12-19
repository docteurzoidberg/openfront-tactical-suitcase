#include "module_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "OTS_MOD_MGR";

#define MAX_MODULES 8

static hardware_module_t *registered_modules[MAX_MODULES];
static uint8_t module_count = 0;

esp_err_t module_manager_init(void) {
    ESP_LOGI(TAG, "Initializing module manager...");
    
    memset(registered_modules, 0, sizeof(registered_modules));
    module_count = 0;
    
    ESP_LOGI(TAG, "Module manager initialized");
    return ESP_OK;
}

esp_err_t module_manager_register(hardware_module_t *module) {
    if (!module) {
        ESP_LOGE(TAG, "Cannot register NULL module");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (module_count >= MAX_MODULES) {
        ESP_LOGE(TAG, "Module registry full");
        return ESP_ERR_NO_MEM;
    }
    
    registered_modules[module_count++] = module;
    ESP_LOGI(TAG, "Registered module: %s", module->name);
    
    return ESP_OK;
}

esp_err_t module_manager_init_all(void) {
    ESP_LOGI(TAG, "Initializing %d modules...", module_count);
    
    for (int i = 0; i < module_count; i++) {
        hardware_module_t *module = registered_modules[i];
        
        if (!module->enabled) {
            ESP_LOGI(TAG, "Module %s is disabled, skipping", module->name);
            continue;
        }
        
        if (module->init) {
            ESP_LOGI(TAG, "Initializing module: %s", module->name);
            esp_err_t ret = module->init();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize module %s: %d", module->name, ret);
                return ret;
            }
        }
    }
    
    ESP_LOGI(TAG, "All modules initialized successfully");
    return ESP_OK;
}

esp_err_t module_manager_update_all(void) {
    for (int i = 0; i < module_count; i++) {
        hardware_module_t *module = registered_modules[i];
        
        if (!module->enabled) {
            continue;
        }
        
        if (module->update) {
            esp_err_t ret = module->update();
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Module %s update failed: %d", module->name, ret);
            }
        }
    }
    
    return ESP_OK;
}

bool module_manager_route_event(const internal_event_t *event) {
    if (!event) {
        return false;
    }
    
    bool handled = false;
    
    for (int i = 0; i < module_count; i++) {
        hardware_module_t *module = registered_modules[i];
        
        if (!module->enabled) {
            continue;
        }
        
        if (module->handle_event && module->handle_event(event)) {
            handled = true;
            ESP_LOGD(TAG, "Event handled by module: %s", module->name);
        }
    }
    
    return handled;
}

uint8_t module_manager_get_count(void) {
    return module_count;
}

hardware_module_t* module_manager_get_module(uint8_t index) {
    if (index >= module_count) {
        return NULL;
    }
    return registered_modules[index];
}
