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

### Decision: integrate discovery into manager

**Status**: ✅ Yes (optional registry + helpers)

**Benefits**:
- **Single place** to maintain “which modules are present” (type/version/caps/node ID/CAN block).
- **Consistent liveness** primitives across firmwares (announce timestamps, “present vs stale”).
- **Simpler module code**: modules can ask the manager “is module X present?” instead of each re-implementing announce parsing.
- **Future-proofing**: adding new CAN modules becomes “define protocol + implement module + discovery announce”; the manager already tracks them.

**Tradeoffs / guardrails**:
- Avoid making the manager discovery-*required*; registry should be **optional** (works fine on a single-node bus too).
- Keep the public header as **generic as possible** (ideally no hard dependency on `can_discovery.h` in the public API).

---

## Deliverables

### Shared component
- `ots-fw-shared/components/can_bus_manager/`
  - `include/can_bus_manager.h`
  - `can_bus_manager.c`
  - `CMakeLists.txt`
  - `idf_component.yml`
  - `COMPONENT_PROMPT.md` (usage + patterns)

### Discovery registry (in manager)
- Add an internal module registry populated from `MODULE_ANNOUNCE` frames.
- Expose a small query API: "is type present?", "get by type/node", "get last-seen".
- Keep the existing `can_bus_manager_discovery_query_all()` helper.

### Refactor fw-main
- Update `ots-fw-main/src/sound_module.c` to use `can_bus_manager`.
- Update `ots-fw-main/src/CMakeLists.txt` dependency list.

### Refactor fw-audiomodule
- Update `ots-fw-audiomodule/src/main.c` to initialize `can_bus_manager`.
- Refactor `ots-fw-audiomodule/src/can_audio_handler.c` to register RX handlers + enqueue TX frames (no module-owned CAN RX loop).
- Update `ots-fw-audiomodule/src/CMakeLists.txt` dependency list.

---

## Status

**Overall**: 🟡 In progress (implementation mostly done; runtime testing pending)

### Tested column semantics

- `UNTESTED`: not validated on real hardware (compile/build may still be done)
- `TESTED`: validated end-to-end on real hardware (at minimum: can traffic observed + expected behavior)

| Item | Status | Tested | Notes |
|------|--------|--------|-------|
| Create this tracking doc | ✅ Done | UNTESTED | Initial file created |
| Define `can_bus_manager` API | ✅ Done | UNTESTED | `include/can_bus_manager.h` (v0.1) |
| Implement TX queue/task | ✅ Done | UNTESTED | Baseline TX task + enqueue-only send |
| Implement RX task + dispatch | ✅ Done | UNTESTED | ID/mask handler routing |
| Add BUS_OFF recovery/backoff | ✅ Done | UNTESTED | Rate-limited recovery in TX task |
| Add optional discovery registry | ✅ Done | UNTESTED | Track `MODULE_ANNOUNCE` and expose registry |
| - Registry data model + storage | ✅ Done | UNTESTED | Fixed-size table keyed by (module_type,node_id) |
| - RX hook for MODULE_ANNOUNCE | ✅ Done | UNTESTED | Parse via `can_discovery_parse_announce()` |
| - Public query API | ✅ Done | UNTESTED | Presence + last-seen + lookup helpers |
| - Header decoupling from discovery | ✅ Done | UNTESTED | `can_discovery.h` kept out of public API |
| Refactor fw-main sound module | ✅ Done | UNTESTED | Uses `can_bus_manager`; no per-module CAN tasks/queues |
| Build esp32-s3-dev | ✅ Done | UNTESTED | `pio run -e esp32-s3-dev` succeeded |
| Refactor fw-audiomodule CAN runtime | ✅ Done | UNTESTED | Uses `can_bus_manager` + handler registration + STATUS task |
| Build esp32-a1s-espidf | ✅ Done | UNTESTED | `pio run -e esp32-a1s-espidf` succeeded |
| Local commits (no push) | ✅ Done | UNTESTED | f709e6c, e753d85, 623120d |
| Hardware test: audio ESP power toggle | ⬜ Not started | UNTESTED | Verify main stays responsive while audio ESP is off/on |
| Hardware test: BUS_OFF recovery | ⬜ Not started | UNTESTED | Force BUS_OFF (if feasible) and confirm recovery/backoff |
| Hardware test: discovery + registry | ⬜ Not started | UNTESTED | Query-all, observe announces, verify registry contents |

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
