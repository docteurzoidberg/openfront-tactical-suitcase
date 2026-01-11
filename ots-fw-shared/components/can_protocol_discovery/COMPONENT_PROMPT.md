# can_protocol_discovery - Component Prompt

## Purpose

`can_protocol_discovery` defines the CAN **boot-time discovery protocol** used to detect modules on the OTS CAN bus.

It is intentionally **protocol-only**:
- Defines CAN IDs and byte layouts for `MODULE_QUERY` / `MODULE_ANNOUNCE`
- Provides small helper functions to **build** and **parse** frames
- Does **not** run tasks, manage timeouts, maintain registries, or send frames by itself

For runtime behavior (TX queueing, RX dispatch, discovery registry), use `can_bus_manager`.

## Files

- `include/can_protocol_discovery.h`
  - Constants:
    - `CAN_ID_MODULE_QUERY` (`0x411`)
    - `CAN_ID_MODULE_ANNOUNCE` (`0x410`)
  - Types:
    - `can_discovery_announce_t`
  - Helpers:
    - `can_discovery_build_query_all()`
    - `can_discovery_build_announce()`
    - `can_discovery_parse_announce()`
    - `can_discovery_get_module_name()`

## Wire Protocol (CANBUS_MESSAGE_SPEC v1.0)

### MODULE_QUERY (0x411)

Direction: Main → All modules (broadcast)  
DLC: 8  
Byte 0: `0xFF` enumerate all modules  
Bytes 1-7: reserved `0x00`

### MODULE_ANNOUNCE (0x410)

Direction: Module → Main  
DLC: 8  
Byte layout:
- Byte 0: module type
- Byte 1: firmware major
- Byte 2: firmware minor
- Byte 3: capabilities bitfield
- Byte 4: CAN block base (e.g. `0x42` for `0x420-0x42F`)
- Byte 5: node id (typically `0x00`)
- Bytes 6-7: reserved `0x00`

## Typical Usage

### Module (responding to query)

- Receive a frame with `id == CAN_ID_MODULE_QUERY`
- Validate byte0==`0xFF`
- Build an announce frame with `can_discovery_build_announce()`
- Send it using your CAN driver or `can_bus_manager_send()`

### Main controller (querying)

- Build a query frame with `can_discovery_build_query_all()`
- Send it on boot (commonly via `can_bus_manager_discovery_query_all()`)
- Parse announces in RX path via `can_discovery_parse_announce()`

## See Also

- Protocol specification: `/prompts/CANBUS_MESSAGE_SPEC.md`
- Runtime manager: `/ots-fw-shared/components/can_bus_manager/COMPONENT_PROMPT.md`
