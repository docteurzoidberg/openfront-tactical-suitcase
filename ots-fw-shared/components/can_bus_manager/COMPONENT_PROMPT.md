# can_bus_manager - Component Prompt

## Purpose

`can_bus_manager` provides shared runtime infrastructure for CAN-based modules:

- Single **TX queue + TX task** (non-blocking for callers)
- Single **RX task** with **ID/mask dispatch** to registered handlers
- Rate-limited **BUS_OFF recovery** attempts using `can_driver_recover()` + `can_driver_start()`

Optional (but recommended):
- **Discovery registry**: parses `MODULE_ANNOUNCE` frames and keeps a small in-memory table of discovered modules (type/version/caps/node/block + last-seen time).

It intentionally does **not** define message formats. Message formats live in protocol components like `can_protocol_discovery`, `can_protocol_audiomodule`, etc.

## Public API

See `include/can_bus_manager.h`.

### Discovery registry helpers

- `can_bus_manager_discovery_query_all()` enqueues a `MODULE_QUERY` broadcast.
- `can_bus_manager_get_module()` / `can_bus_manager_is_module_present()` let modules and the main controller check whether a CAN module is currently present (based on last announce).
- `can_bus_manager_list_modules()` copies the current registry table for diagnostics/UI.

## Usage Pattern (fw-main)

1. Initialize once at boot:
   - `can_bus_manager_init(&config)`
2. For each module:
   - register RX handlers for CAN IDs it cares about
   - build outbound frames using `can_<module>` helpers
   - send via `can_bus_manager_send()`

## Notes

- `can_bus_manager_send()` is enqueue-only. It returns `ESP_ERR_TIMEOUT` if the queue is full.
- If the bus is missing ACKs (e.g., only one powered node), `can_driver_send()` may return `ESP_ERR_TIMEOUT`. This is treated as a timeout (not a fatal error).
- Non-timeout send errors trigger recovery attempts with backoff.

- The registry is updated opportunistically whenever a `MODULE_ANNOUNCE` frame is received. It does not generate traffic by itself.
