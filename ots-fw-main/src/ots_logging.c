#include "ots_logging.h"

#include "config.h"
#include "esp_log.h"

#ifndef OTS_LOG_LEVEL
// Match ESP-IDF's esp_log_level_t values:
// 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE
#define OTS_LOG_LEVEL 3
#endif

esp_err_t ots_logging_init(void) {
    int level = (int)OTS_LOG_LEVEL;
    if (level < 0) {
        level = 0;
    } else if (level > 5) {
        level = 5;
    }

    esp_log_level_set("*", (esp_log_level_t)level);
    return ESP_OK;
}
