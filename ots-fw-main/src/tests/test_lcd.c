/*
 * LCD Display Test (16x2 HD44780 via PCF8574 @ 0x27)
 * 
 * Tests LCD display with various patterns and text.
 * Uses existing lcd_driver from firmware.
 * 
 * Expected hardware:
 *   - 16x2 LCD with I2C backpack (PCF8574) at 0x27
 *   - HD44780 compatible display
 * 
 * Test sequence:
 *   1. Backlight test (on/off)
 *   2. Text display (various patterns)
 *   3. Troop display format (production use case)
 *   4. Scrolling counter
 *   5. Character set test
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lcd_driver.h"
#include "i2c_bus.h"

static const char *TAG = "TEST_LCD";

void test_backlight(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Backlight Test ===");
    
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("Backlight Test");
    lcd_set_cursor(0, 1);
    lcd_write_string("Watch screen...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Note: Backlight control would require extending lcd_driver.h
    // For now, just display message
    ESP_LOGI(TAG, "Backlight should be ON");
    ESP_LOGI(TAG, "If not visible, check:");
    ESP_LOGI(TAG, "  - LCD power (5V)");
    ESP_LOGI(TAG, "  - Contrast adjustment (potentiometer on back)");
    ESP_LOGI(TAG, "  - I2C connections");
}

void test_text_display(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Text Display Test ===");
    
    // Test 1: Welcome message
    ESP_LOGI(TAG, "Test 1: Welcome message");
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("  LCD TEST OK  ");
    lcd_set_cursor(0, 1);
    lcd_write_string("  Hardware v1  ");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Test 2: Full width text
    ESP_LOGI(TAG, "Test 2: Full width (16 chars)");
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("0123456789ABCDEF");
    lcd_set_cursor(0, 1);
    lcd_write_string("GHIJKLMNOPQRSTUV");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Test 3: Numbers and symbols
    ESP_LOGI(TAG, "Test 3: Numbers and symbols");
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("0123456789      ");
    lcd_set_cursor(0, 1);
    lcd_write_string("!@#$%^&*()+-=<> ");
    vTaskDelay(pdMS_TO_TICKS(3000));
}

void test_troop_format(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Troop Display Format ===");
    ESP_LOGI(TAG, "Production format used in Troops Module");
    
    // Test various troop counts
    struct {
        const char *line1;
        const char *line2;
        int delay_ms;
    } tests[] = {
        {"  120K / 1.1M  ", "50% (60K)       ", 3000},
        {"   1.5M / 5M   ", "75% (1.1M)      ", 3000},
        {"  500 / 10K    ", "25% (125)       ", 3000},
        {"  5.2B / 10B   ", "100% (5.2B)     ", 3000},
    };
    
    for (int i = 0; i < 4; i++) {
        ESP_LOGI(TAG, "Example %d: %s / %s", i+1, tests[i].line1, tests[i].line2);
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_write_string(tests[i].line1);
        lcd_set_cursor(0, 1);
        lcd_write_string(tests[i].line2);
        vTaskDelay(pdMS_TO_TICKS(tests[i].delay_ms));
    }
}

void test_scrolling_counter(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Scrolling Counter Test ===");
    ESP_LOGI(TAG, "Watch counter increment rapidly");
    
    char line1[17], line2[17];
    
    for (int i = 0; i <= 100; i += 5) {
        snprintf(line1, sizeof(line1), "Counter: %3d    ", i);
        snprintf(line2, sizeof(line2), "Value: %3d      ", i * 10);
        
        lcd_set_cursor(0, 0);
        lcd_write_string(line1);
        lcd_set_cursor(0, 1);
        lcd_write_string(line2);
        
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void test_alignment(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Text Alignment Test ===");
    
    // Left aligned
    ESP_LOGI(TAG, "Left aligned");
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("Left aligned    ");
    lcd_set_cursor(0, 1);
    lcd_write_string("Text here       ");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Right aligned
    ESP_LOGI(TAG, "Right aligned");
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("   Right aligned");
    lcd_set_cursor(0, 1);
    lcd_write_string("      Text here ");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Centered
    ESP_LOGI(TAG, "Centered");
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("   Centered!    ");
    lcd_set_cursor(0, 1);
    lcd_write_string("  Text here!    ");
    vTaskDelay(pdMS_TO_TICKS(2000));
}

void test_cursor_positioning(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Cursor Position Test ===");
    ESP_LOGI(TAG, "Writing individual characters at specific positions");
    
    lcd_clear();
    
    // Draw a border pattern
    lcd_set_cursor(0, 0);
    lcd_write_string("################");
    lcd_set_cursor(0, 1);
    lcd_write_string("################");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Write text in middle
    lcd_set_cursor(2, 0);
    lcd_write_string("  Position  ");
    lcd_set_cursor(2, 1);
    lcd_write_string("    Test    ");
    vTaskDelay(pdMS_TO_TICKS(2000));
}

void app_main(void) {
    ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║    OTS LCD Display Test               ║");
    ESP_LOGI(TAG, "║    16x2 LCD @ 0x27                    ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    ESP_ERROR_CHECK(ots_i2c_bus_init());
    
    ESP_LOGI(TAG, "Initializing LCD display...");
    esp_err_t ret = lcd_init(LCD_I2C_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LCD!");
        ESP_LOGE(TAG, "Check:");
        ESP_LOGE(TAG, "  - LCD connected to I2C bus");
        ESP_LOGE(TAG, "  - I2C address is 0x27 (or 0x3F)");
        ESP_LOGE(TAG, "  - 5V power connected");
        ESP_LOGE(TAG, "  - Contrast adjusted (potentiometer)");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    ESP_LOGI(TAG, "LCD initialized successfully");
    ESP_LOGI(TAG, "");
    
    // Initial display
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string(" OTS LCD Test   ");
    lcd_set_cursor(0, 1);
    lcd_write_string(" Starting...    ");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    int cycle = 1;
    while (1) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
        ESP_LOGI(TAG, "║ Test Cycle %d                          ║", cycle++);
        ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
        
        test_backlight();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        test_text_display();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        test_troop_format();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        test_alignment();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        test_cursor_positioning();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        test_scrolling_counter();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "╔═══════════════════════════════════════╗");
        ESP_LOGI(TAG, "║ Cycle Complete                        ║");
        ESP_LOGI(TAG, "╚═══════════════════════════════════════╝");
        
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_write_string("Test Complete!  ");
        lcd_set_cursor(0, 1);
        lcd_write_string("Restarting...   ");
        
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
