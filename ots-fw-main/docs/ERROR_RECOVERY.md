# I/O Expander Error Recovery System

## Overview

The OTS firmware implements comprehensive error recovery for MCP23017 I/O expander chips to ensure robust operation even when hardware communication fails temporarily.

## Features

### 1. **Initialization Retry Logic**
- Attempts to initialize each board up to **5 times** with exponential backoff
- Initial retry delay: **100ms**
- Maximum retry delay: **5000ms** (5 seconds)
- Each retry doubles the delay: 100ms → 200ms → 400ms → 800ms → 1600ms
- Tests communication with a read operation after device registration

### 2. **Health Monitoring**
Each board tracks:
- `initialized` - Board successfully initialized
- `healthy` - Board currently operational
- `error_count` - Total errors since boot
- `consecutive_errors` - Errors in a row (resets on success)
- `recovery_count` - Number of successful recoveries
- `last_error_time` - Timestamp of last error (milliseconds)
- `last_health_check` - Timestamp of last health check

### 3. **Automatic Error Detection**
Every I/O operation (`digital_read`, `digital_write`) automatically:
- Records errors on I2C communication failure
- Records successes to reset consecutive error counter
- Marks board unhealthy after **3 consecutive errors**
- Logs warnings when boards become unhealthy

### 4. **Periodic Health Checks**
The I/O task performs health checks every **10 seconds**:
- Tests read from IODIRA register on each board
- Skips boards checked within last 10 seconds (configurable)
- Automatically attempts recovery if health check fails
- Logs results for monitoring

### 5. **Recovery Mechanisms**

#### Automatic Recovery
```c
// In io_task loop (every 10 seconds)
if (!io_expander_health_check()) {
    uint8_t recovered = io_expander_attempt_recovery();
    ESP_LOGI(TAG, "Recovered %d board(s)", recovered);
}
```

#### Manual Recovery
```c
// Recover specific board
esp_err_t result = io_expander_reinit_board(0);

// Recover all unhealthy boards
uint8_t recovered = io_expander_attempt_recovery();
```

### 6. **Recovery Callbacks**
Register a callback to be notified when boards recover:
```c
static void handle_io_recovery(uint8_t board, bool was_down) {
    ESP_LOGI(TAG, "Board #%d recovered!", board);
    // Reinitialize module I/O, update LED states, etc.
}

io_expander_set_recovery_callback(handle_io_recovery);
```

### 7. **Health Status Queries**
```c
io_expander_health_t status;
if (io_expander_get_health(0, &status)) {
    ESP_LOGI(TAG, "Board 0: errors=%lu, recoveries=%lu",
             status.error_count, status.recovery_count);
}
```

## Configuration Constants

Defined in `include/io_expander.h`:

| Constant | Value | Description |
|----------|-------|-------------|
| `IO_EXPANDER_MAX_RETRIES` | 5 | Max initialization attempts |
| `IO_EXPANDER_INITIAL_RETRY_DELAY_MS` | 100 | Starting retry delay |
| `IO_EXPANDER_MAX_RETRY_DELAY_MS` | 5000 | Maximum retry delay |
| `IO_EXPANDER_HEALTH_CHECK_INTERVAL_MS` | 10000 | Health check frequency |
| `IO_EXPANDER_MAX_CONSECUTIVE_ERRORS` | 3 | Errors before unhealthy |

## Error Flow Example

```
1. Button read fails → record_error(board)
   - error_count: 0 → 1
   - consecutive_errors: 0 → 1
   - Board still healthy

2. LED write fails → record_error(board)
   - error_count: 1 → 2
   - consecutive_errors: 1 → 2
   - Board still healthy

3. Another read fails → record_error(board)
   - error_count: 2 → 3
   - consecutive_errors: 2 → 3
   - Board marked UNHEALTHY (≥3 consecutive)

4. Health check (10s later) detects unhealthy board
   - Calls io_expander_attempt_recovery()
   - Removes old device handle
   - Reinitializes with retry logic (up to 5 attempts)

5. If successful:
   - Board marked healthy
   - consecutive_errors reset to 0
   - recovery_count incremented
   - Recovery callback fired
   - Module I/O reconfigured
   
6. Next successful operation:
   - record_success() confirms recovery
   - Logs "Board #X recovered"
```

## Benefits

### Robustness
- System continues operating with partial I/O expander failure
- Automatic recovery without manual intervention
- Graceful degradation instead of hard failures

### Observability
- Detailed error logging with ESP_LOG
- Error counters for debugging
- Health status queries for monitoring
- Recovery events tracked and logged

### Maintainability
- Clear separation of concerns (error tracking vs. I/O operations)
- Configurable constants for tuning
- Callback system for module-specific recovery actions

## Integration Points

### 1. Main Application (`main.c`)
```c
// Register recovery callback
io_expander_set_recovery_callback(handle_io_expander_recovery);

// Callback reconfigures module I/O after recovery
static void handle_io_expander_recovery(uint8_t board, bool was_down) {
    module_io_reinit();  // Reconfigure pins
}
```

### 2. I/O Task (`io_task.c`)
```c
// Every 10 seconds in I/O loop
if (!io_expander_health_check()) {
    io_expander_attempt_recovery();
}
```

### 3. Module I/O (`module_io.c`)
```c
// Reinitialize pin configurations after recovery
esp_err_t module_io_reinit(void) {
    return module_io_init();  // Rerun initialization
}
```

## Testing

### Simulate I2C Failure
1. Disconnect I2C bus during operation
2. Watch logs for error detection
3. Reconnect bus
4. Verify automatic recovery within 10 seconds

### Monitor Health
```c
// Add to main loop or debug task
io_expander_health_t status;
for (uint8_t i = 0; i < 2; i++) {
    if (io_expander_get_health(i, &status)) {
        ESP_LOGI(TAG, "Board %d: healthy=%d errors=%lu recoveries=%lu",
                 i, status.healthy, status.error_count, status.recovery_count);
    }
}
```

## Future Enhancements

- [ ] Add metrics reporting via WebSocket
- [ ] Configurable health check interval via settings
- [ ] I2C bus reset on persistent failures
- [ ] Error rate trending (errors per minute)
- [ ] Dashboard UI for I/O expander health status
- [ ] NVS storage for persistent error statistics

## Related Files

- `include/io_expander.h` - Public API and configuration
- `src/io_expander.c` - Implementation with retry and recovery logic
- `src/io_task.c` - Periodic health checks in I/O loop
- `src/main.c` - Recovery callback registration
- `src/module_io.c` - Pin reconfiguration after recovery
- `docs/CHANGELOG.md` - Version history and changes

