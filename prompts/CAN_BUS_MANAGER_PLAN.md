# CAN Bus Manager Refactor Plan (Tracking)

**Date started**: 2026-01-10  
**Owner**: GitHub Copilot (GPT-5.2)  
**Scope**: Introduce a shared CAN bus runtime manager for all current/future CAN-based modules.

## Goals

- **Single CAN runtime** per firmware: one RX task, one TX task/queue.
- **Non-blocking**: module code must never block on CAN transmit (e.g. 100ms `twai_transmit()` timeout).
- **Reusable**: new CAN modules should only implement protocol encode/decode + register handlers.
- **Resilient**: automatic BUS_OFF recovery with backoff.
- **Discovery-friendly**: optional integration with `can_discovery` for module registry.

## Constraints / Non-goals

- Do **not** redesign CAN IDs or message formats.
- Keep `can_driver` as low-level TWAI abstraction.
- Keep `can_discovery` as protocol helper (query/announce parsing/building).
- Keep module-specific protocol components (e.g. `can_audiomodule`) as encode/decode only.

---

## Deliverables

### Shared component
- `ots-fw-shared/components/can_bus_manager/`
  - `include/can_bus_manager.h`
  - `can_bus_manager.c`
  - `CMakeLists.txt`
  - `idf_component.yml`
  - `COMPONENT_PROMPT.md` (usage + patterns)

### Refactor fw-main
- Update `ots-fw-main/src/sound_module.c` to use `can_bus_manager`.
- Update `ots-fw-main/src/CMakeLists.txt` dependency list.

---

## Status

**Overall**: 🟡 In progress

| Item | Status | Notes |
|------|--------|-------|
| Create this tracking doc | ✅ Done | Initial file created |
| Define `can_bus_manager` API | ✅ Done | `include/can_bus_manager.h` (v0.1) |
| Implement TX queue/task | ✅ Done | Baseline TX task + enqueue-only send |
| Implement RX task + dispatch | ✅ Done | ID/mask handler routing |
| Add BUS_OFF recovery/backoff | ✅ Done | Rate-limited recovery in TX task |
| Add optional discovery registry | ⬜ Not started | Track `MODULE_ANNOUNCE` and expose registry |
| Refactor fw-main sound module | ✅ Done | Uses `can_bus_manager`; no per-module CAN tasks/queues |
| Build esp32-s3-dev | ✅ Done | `pio run -e esp32-s3-dev` succeeded |
| Local commits (no push) | ⬜ Not started | One or more commits |

---

## Implementation Outline

### 1) API (v1)

- `can_bus_manager_init(const can_config_t *cfg)` (idempotent)
- `can_bus_manager_deinit()`
- `can_bus_manager_send(const can_frame_t *frame)` (enqueue-only)
- `can_bus_manager_register_handler(uint16_t id, uint16_t mask, cb, ctx)`
- `can_bus_manager_unregister_handler(...)` (optional)
- `can_bus_manager_discovery_query_all()` (enqueue MODULE_QUERY)

### 2) Internal architecture

- **TX task**: pulls items from queue and calls `can_driver_send()`.
  - On timeout: increment stats, apply drop policy (do not block callers).
  - On other errors: log TWAI status and try recovery with backoff.
- **RX task**: reads with `can_driver_receive()` and dispatches to matching handlers.
- **Handler table**: fixed-size array (no heap).

### 3) Module integration pattern

- Module `init()`:
  - ensure `can_bus_manager_init()` is called
  - register RX handlers for its CAN IDs
- Module `update()`:
  - handle liveness (status timestamps) at module level
- Module actions:
  - build frames using its `can_<module>` protocol component
  - send via `can_bus_manager_send()`

---

## Notes / Decisions Log

- 2026-01-10: Prefer **new shared component** over stuffing routing/policy into `can_driver`.
- 2026-01-10: Use FreeRTOS tick uptime for recovery backoff (avoid `esp_timer.h` dependency in shared component build context).
