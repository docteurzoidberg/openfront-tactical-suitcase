# Minimal CAN Hardware Test Firmware

Bare-metal TWAI/CAN hardware validation for ESP32-S3 and ESP32-A1S.

## Purpose

Test CAN TX/RX at the lowest level to isolate hardware issues.

## Build

```bash
# ESP32-S3
pio run -e esp32-s3 -t upload && pio device monitor

# ESP32-A1S  
pio run -e esp32-a1s -t upload && pio device monitor
```

## Test Stages

1. **TWAI Init**: Verify driver initialization
2. **Self-Test**: Loopback mode validation
3. **TX Test**: Send frames, measure voltage on TX pin
4. **RX Test**: Receive frames from other device
5. **Full Test**: Bidirectional communication

## GPIO Pins

- **ESP32-S3**: TX=GPIO5, RX=GPIO4
- **ESP32-A1S**: TX=GPIO5, RX=GPIO18
