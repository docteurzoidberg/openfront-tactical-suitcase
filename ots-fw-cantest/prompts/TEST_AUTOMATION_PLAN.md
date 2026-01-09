# OTS CAN Test Automation Plan (cantest)

This file consolidates the automation plan previously scattered under `ots-fw-cantest/tools/*.md`.

## Mission

Provide a repeatable, automation-friendly path to validate OTS CAN bus behavior across:

- **Phase 2.5** (baseline): `cantest ↔ cantest` (two-devices)
- **Phase 3** (production):
  - **3.2**: `cantest controller → production ots-fw-audiomodule`
  - **3.3**: `production ots-fw-main → cantest audio simulator`

The plan assumes the CAN message formats are defined in the repo-level spec:
- [/prompts/CANBUS_MESSAGE_SPEC.md](../../prompts/CANBUS_MESSAGE_SPEC.md)

## Non-goals

- Redefining protocol types in this repo (use the spec + shared component sources).
- Hiding hardware issues with software workarounds.

## Architecture: composable toolchain

Layering concept:

1. **Foundation** (Python helpers)
   - `tools/lib/device.py` (serial device helper)
   - `tools/lib/protocol.py` (CAN frame decode)

2. **Command / capture**
   - `can_monitor.py` (serial → JSONL frames)
   - `can_send.py` / `send_cantest_command.py` (drive cantest CLI)

3. **Analysis**
   - `can_decode.py` (JSONL frames → decoded JSONL)

4. **Scenario runners**
   - `test_two_device.py`
   - `test_production_audio.py`
   - `test_production_main.py`

Design principles:
- Prefer machine-parsable outputs (JSON Lines) where possible.
- Prefer *small tools* that can be piped/combined over monolith scripts.

## Phases and acceptance criteria

### Phase 2.5 — Baseline (cantest ↔ cantest)

Goal: validate that the tooling and protocol decoding works on a known-good, controlled setup.

Primary runner:
- `tools/test_two_device.py`

Acceptance:
- MODULE_QUERY (0x411) and MODULE_ANNOUNCE (0x410) observed on both sides.
- Audio protocol messages can be sent/decoded consistently.
- Logs captured deterministically.

Status (historical): Phase 2.5 marked complete in the original docs.

### Phase 3.2 — Production audio module validation

Goal: validate that production `ots-fw-audiomodule` matches the CAN spec and behaves like the baseline.

Primary runner:
- `tools/test_production_audio.py`

Acceptance:
- Discovery works (query + announce).
- Audio module handles PLAY/STOP paths per spec.
- Serial output confirms audio actions (where applicable).

Status (historical): Phase 3.2 marked complete (3/3) in prior docs.

Critical historical fixes to preserve:
- Shared component fix: PLAY_SOUND_ACK format aligned to spec.
- Test parser fix: `decode_frame()` returned nested dict; test script updated to match.
- Config alignment: main and audiomodule CAN pins/bitrate aligned during validation.

### Phase 3.3 — Production main controller validation

Goal: validate production `ots-fw-main` can discover and interact with an audio module (simulated by cantest).

Primary runner:
- `tools/test_production_main.py`

Acceptance:
- On boot, production main sends MODULE_QUERY (0x411).
- cantest audio simulator receives query and responds with MODULE_ANNOUNCE (0x410).
- production main receives announce and proceeds accordingly.

Status (historical): Phase 3.3 in progress and blocked by physical-layer verification issues.

## Known blocker: physical layer (Phase 3.3)

Symptom cluster described in the legacy docs:
- TWAI goes **BUS_OFF** shortly after boot or after attempts to transmit.
- No observed RX traffic on the other device.

Primary checklist docs:
- [HARDWARE_DEBUG_CHECKLIST.md](HARDWARE_DEBUG_CHECKLIST.md)
- [FRESH_START_CHECKLIST.md](FRESH_START_CHECKLIST.md)
- [MODULE_DEBUG_GUIDE.md](MODULE_DEBUG_GUIDE.md)
- [diagnose_hardware.md](diagnose_hardware.md)

Consolidation recommendation: treat these as **debug playbooks** and ensure the tracker captures which steps were executed.

Typical highest-value checks:
- Termination: 120Ω at each end, ~60Ω measured across CANH/CANL with both ends connected.
- Transceiver enable/standby pins (e.g., TJA1050 “S”/standby tied correctly).
- CANH/CANL wiring continuity and not swapped.
- Common ground between nodes.
- Bitrate alignment across devices.

## Workflow (recommended)

1. **Baseline first (Phase 2.5)**
   - Confirm cantest↔cantest works before debugging production hardware.

2. **Production audio (Phase 3.2)**
   - Confirm cantest controller can drive production audiomodule reliably.

3. **Production main (Phase 3.3)**
   - Use a minimal discovery-only check first, then expand.
   - Prefer observing **actual CAN frames** on both ports.

## Tooling references

See per-tool docs:
- [prompts/tools/INDEX.md](tools/INDEX.md)

## Legacy source documents

The old planning/status markdown files in `ots-fw-cantest/tools/` were fully consolidated into this plan and then removed.
If you need historical notes, use `git log` / `git show`.
