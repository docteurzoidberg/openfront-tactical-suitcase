#ifndef OTS_COMMON_H
#define OTS_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include "cJSON.h"

/**
 * @brief OTS Project Constants
 * 
 * Project-wide naming and identification constants
 */

// Project identification
#define OTS_PROJECT_NAME        "OpenFront Tactical Suitcase"
#define OTS_PROJECT_ABBREV      "OTS"
#define OTS_FIRMWARE_NAME       "ots-fw-main"
#define OTS_FIRMWARE_VERSION    "0.1.0"

// Logging TAG prefix convention: OTS_<COMPONENT>
// All logging tags should follow this pattern for easy filtering
#define OTS_TAG_PREFIX          "OTS_"

/**
 * @brief Common JSON parsing helpers
 * 
 * These functions reduce code duplication across modules
 */

/**
 * @brief Parse unit ID from JSON data
 * 
 * Looks for "nukeUnitID" or "unitID" in JSON object
 * 
 * @param data_json JSON string to parse
 * @return uint32_t Unit ID, or 0 if not found/error
 */
static inline uint32_t ots_parse_unit_id(const char *data_json) {
    if (!data_json || *data_json == '\0') {
        return 0;
    }
    
    cJSON *root = cJSON_Parse(data_json);
    if (!root) {
        return 0;
    }
    
    uint32_t unit_id = 0;
    cJSON *id = cJSON_GetObjectItem(root, "nukeUnitID");
    if (!id) {
        id = cJSON_GetObjectItem(root, "unitID");
    }
    
    if (id && cJSON_IsNumber(id)) {
        unit_id = (uint32_t)id->valueint;
    }
    
    cJSON_Delete(root);
    return unit_id;
}

/**
 * @brief Parse integer value from JSON object
 * 
 * @param root JSON object (must not be NULL)
 * @param key Key to look up
 * @param default_value Default value if not found
 * @return int32_t Parsed value or default
 */
static inline int32_t ots_json_get_int(cJSON *root, const char *key, int32_t default_value) {
    if (!root || !key) return default_value;
    
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item && cJSON_IsNumber(item)) {
        return item->valueint;
    }
    return default_value;
}

/**
 * @brief Parse string value from JSON object
 * 
 * @param root JSON object (must not be NULL)
 * @param key Key to look up
 * @return const char* String value or NULL if not found
 */
static inline const char* ots_json_get_string(cJSON *root, const char *key) {
    if (!root || !key) return NULL;
    
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item && cJSON_IsString(item)) {
        return item->valuestring;
    }
    return NULL;
}

/**
 * @brief Parse boolean value from JSON object
 * 
 * @param root JSON object (must not be NULL)
 * @param key Key to look up
 * @param default_value Default value if not found
 * @return bool Parsed value or default
 */
static inline bool ots_json_get_bool(cJSON *root, const char *key, bool default_value) {
    if (!root || !key) return default_value;
    
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item && cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }
    return default_value;
}

#endif // OTS_COMMON_H
