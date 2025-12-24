# Build & Test Prompt (ots-fw-main)

## Default Tooling

- Prefer **PlatformIO** builds.
- Run the smallest build that proves your change.

### Commands

- Build production firmware:
  - `cd ots-fw-main && pio run -e esp32-s3-dev`
- Upload production firmware:
  - `pio run -e esp32-s3-dev -t upload`
- Monitor:
  - `pio device monitor --port /dev/ttyACM0 --baud 115200`

### Test Environments

Use the dedicated environments (see `platformio.ini` and `docs/TESTING.md`):

- `pio run -e test-i2c`
- `pio run -e test-outputs`
- `pio run -e test-inputs`
- `pio run -e test-adc`
- `pio run -e test-lcd`
- `pio run -e test-websocket`

## Important Reminders

- Ignore this non-blocking warning unless explicitly asked:
  - `esp_idf_size: error: unrecognized arguments: --ng`
- Treat a build as **good** when compile+link succeed and `pio run` exits successfully.

## Build System Expectations

- Test selection is controlled by PlatformIO `build_flags` (`-DTEST_*`).
- Do **not** reintroduce scripts that swap `src/CMakeLists.txt`.

## When a Build Fails

- If it’s a link error like missing `app_main`, confirm you built the correct `test-*` env.
- Check `src/CMakeLists.txt` test selection logic before touching unrelated code.
- Keep fixes focused on the failure; don’t refactor broadly.
