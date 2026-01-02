#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>

// NVS-backed device metadata.
//
// - Owner name is set during web onboarding (and later used in hardware diagnostics).
// - Serial number defaults to a build-time value, but can be overridden in NVS.

esp_err_t device_settings_init(void);

bool device_settings_owner_exists(void);
esp_err_t device_settings_get_owner(char *out, size_t out_len);
esp_err_t device_settings_set_owner(const char *owner);

esp_err_t device_settings_get_serial(char *out, size_t out_len);
esp_err_t device_settings_set_serial(const char *serial);

// Factory reset: clear all device settings (owner, serial).
esp_err_t device_settings_factory_reset(void);
