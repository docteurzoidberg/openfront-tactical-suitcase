/*
 * ADC Test (ADS1015 @ 0x48)
 * 
 * Continuously reads all 4 ADC channels and displays values.
 * Uses existing adc_driver from firmware.
 * 
 * Expected hardware:
 *   - ADS1015 at I2C address 0x48
 *   - Potentiometer/voltage source on AIN0-AIN3
 * 
 * Test verification:
 *   - Connect potentiometer to AIN0
 *   - Rotate potentiometer and watch values change
 *   - Values should range from 0 to ~4095 (12-bit)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "adc_driver.h"
#include "i2c_bus.h"

#include "ots_logging.h"

static const char *TAG = "TEST_ADC";

float adc_to_voltage(int16_t adc_value) {
    // ADS1015 is 12-bit, ±4.096V range
    // 4095 counts = 4.096V
    return (adc_value * 4.096f) / 4095.0f;
}

uint8_t adc_to_percent(int16_t adc_value) {
    // Convert to percentage (0-100%)
    int percent = (adc_value * 100) / 4095;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return (uint8_t)percent;
}

void test_single_channel(uint8_t channel) {
    int16_t raw = ads1015_read_channel(channel);
    
    if (raw < 0) {
        ESP_LOGE(TAG, "  AIN%d: Read error", channel);
        return;
    }
    
    float voltage = adc_to_voltage(raw);
    uint8_t percent = adc_to_percent(raw);
    
    ESP_LOGI(TAG, "  AIN%d: %4d (%.3fV, %3d%%)", channel, raw, voltage, percent);
}

void test_all_channels(void) {
    // Read all 4 channels
    for (uint8_t ch = 0; ch < 4; ch++) {
        test_single_channel(ch);
        vTaskDelay(pdMS_TO_TICKS(10));  // Small delay between channels
    }
}

void test_continuous_reading(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Continuous Reading (10 samples) ===");
    
    for (int i = 0; i < 10; i++) {
        int16_t raw = ads1015_read_channel(ADS1015_CHANNEL_AIN0);
        if (raw >= 0) {
            float voltage = adc_to_voltage(raw);
            printf("\r[%2d] AIN0: %4d (%.3fV, %3d%%)  ", 
                   i+1, raw, voltage, adc_to_percent(raw));
            fflush(stdout);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    printf("\n");
}

void test_voltage_ranges(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Voltage Range Test ===");
    ESP_LOGI(TAG, "Expected ranges:");
    ESP_LOGI(TAG, "  0V    → 0     ADC counts");
    ESP_LOGI(TAG, "  1V    → ~1000 ADC counts");
    ESP_LOGI(TAG, "  2V    → ~2000 ADC counts");
    ESP_LOGI(TAG, "  3.3V  → ~3300 ADC counts");
    ESP_LOGI(TAG, "  4.096V→ ~4095 ADC counts");
    ESP_LOGI(TAG, "");
    
    int16_t raw = ads1015_read_channel(ADS1015_CHANNEL_AIN0);
    if (raw >= 0) {
        float voltage = adc_to_voltage(raw);
        ESP_LOGI(TAG, "Current AIN0: %d counts = %.3fV", raw, voltage);
        
        if (raw < 100) {
            ESP_LOGI(TAG, "  ✓ Near 0V (very low voltage)");
        } else if (raw < 1200) {
            ESP_LOGI(TAG, "  ✓ ~1V range");
        } else if (raw < 2200) {
            ESP_LOGI(TAG, "  ✓ ~2V range");
        } else if (raw < 3500) {
            ESP_LOGI(TAG, "  ✓ ~3.3V range (typical MCU voltage)");
        } else {
            ESP_LOGI(TAG, "  ✓ High voltage (near 4.096V max)");
        }
    }
}

void app_main(void) {
    (void)ots_logging_init();

    ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║    OTS ADC Test                       ║");
    ESP_LOGI(TAG, "║    ADS1015 @ 0x48                     ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    ESP_ERROR_CHECK(ots_i2c_bus_init());
    
    ESP_LOGI(TAG, "Initializing ADS1015 ADC...");
    esp_err_t ret = ads1015_init(ADS1015_I2C_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADS1015!");
        ESP_LOGE(TAG, "Check:");
        ESP_LOGE(TAG, "  - ADS1015 connected to I2C bus");
        ESP_LOGE(TAG, "  - I2C address is 0x48");
        ESP_LOGE(TAG, "  - Power supply connected");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    ESP_LOGI(TAG, "ADS1015 initialized successfully");
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "Instructions:");
    ESP_LOGI(TAG, "  - Connect potentiometer to AIN0 (main test)");
    ESP_LOGI(TAG, "  - Optional: Connect to AIN1-3 for multi-channel test");
    ESP_LOGI(TAG, "  - 12-bit resolution: 0-4095 counts");
    ESP_LOGI(TAG, "  - Voltage range: 0-4.096V");
    ESP_LOGI(TAG, "");
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    int cycle = 1;
    while (1) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
        ESP_LOGI(TAG, "║ Test Cycle %d                          ║", cycle++);
        ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
        
        // Test 1: All channels snapshot
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "=== All Channels Snapshot ===");
        test_all_channels();
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // Test 2: Continuous reading (watch changes)
        test_continuous_reading();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Test 3: Voltage range interpretation
        test_voltage_ranges();
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "Cycle complete. Next cycle in 3 seconds...");
        ESP_LOGI(TAG, "Rotate potentiometer to see values change");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
