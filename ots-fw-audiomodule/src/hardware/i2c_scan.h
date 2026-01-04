/**
 * @file i2c_scan.h
 * @brief I2C Bus Scanner Utility
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Scan I2C bus and print all detected devices
 * 
 * Scans addresses 0x00-0x7F and logs which devices respond
 */
void i2c_scan_bus(void);

#ifdef __cplusplus
}
#endif
