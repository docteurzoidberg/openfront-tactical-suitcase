# cantest — Test Status / Tracker / Logs

This file is the single place to record *what was run*, *with what hardware*, and *what happened*.

## Current phase status (carry-forward)

- Phase 2.5 (cantest ↔ cantest): **complete** (baseline validated)
- Phase 3.2 (cantest → production audiomodule): **complete** (validated 3/3)
- Phase 3.3 (production main → cantest audio sim): **in progress / blocked** (hardware verification)

If this status changes, update this section first.

## Logs already in repo

Located under `ots-fw-cantest/tools/`:

- `phase3_scenario1_test.log`
- `phase3_scenario1_validated.log`
- `phase3_scenario1_fixed.log`
- `phase3_scenario1_fixed_final.log`
- `phase3_3_test_results.log`
- `phase3_3_final_test.log`
- `phase3_scenario2_results.log`
- `hardware_revalidation.log`

Rule: when you run a scenario runner, capture output to a new log file and link it below.

## Run template

Copy/paste this template for each run and fill it out.

### Run: <YYYY-MM-DD HH:MM> — <short title>

**Goal**:
- 

**Hardware**:
- Nodes:
  - Node A: <board> <transceiver> TX/RX pins <...>
  - Node B: <board> <transceiver> TX/RX pins <...>
- Bitrate: <e.g., 125k>
- Termination measured across CANH/CANL: <ohms>
- Standby/enable pin wiring: <details>
- Common GND confirmed: <yes/no>

**Firmware / versions**:
- cantest env + commit/ref: <...>
- ots-fw-main env + commit/ref: <...>
- ots-fw-audiomodule env + commit/ref: <...>

**Ports**:
- Node A serial: <...>
- Node B serial: <...>

**Commands**:
- <exact commands/scripts run>

**Result summary**:
- PASS/FAIL:
- Key observations:
  - 

**Artifacts**:
- Log file: <path>
- Any captured JSONL: <path>

**Next action**:
- 

## Quick triage rubric

When diagnosing failures, record which bucket it falls into:

- **Protocol / decoding mismatch**: frames seen, decoded wrong or unexpected.
- **App logic mismatch**: discovery happens, but state machine/response handling wrong.
- **Physical layer**: no frames seen, BUS_OFF, wiring/termination/transceiver enable.

## When to escalate to hardware debugging

If any of these occurs, treat it as physical-layer until proven otherwise:
- TWAI state transitions to BUS_OFF.
- TX count increases but peer RX stays flat.
- Both devices show errors immediately when attempting traffic.

Recommended: run loopback tests first, then two-device baseline, then production.

Tool docs index:
- [prompts/tools/INDEX.md](tools/INDEX.md)
