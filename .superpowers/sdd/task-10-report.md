# Task 10 Report: NVS Persistence (Long-term Stats + Config)

## Status: DONE

## What was created

- `src/nvs_store.h` — written exactly per spec.
- `src/nvs_store.c` — written exactly per spec (`nvs_flash_init` with erase/reinit fallback on
  `ESP_ERR_NVS_NO_FREE_PAGES`/`ESP_ERR_NVS_NEW_VERSION_FOUND`; blob load/save via `nvs_open`/`nvs_get_blob`/
  `nvs_set_blob`/`nvs_commit`/`nvs_close`; stats load falls back to `lambda_stats_reset` on read failure or
  failed `lambda_stats_validate` (CRC/version check); config load falls back to `si_default_calibration` on
  read failure; `nvs_store_reset_longterm` resets in-memory and persists immediately).
- `src/adc_task.c` — 3 edits per spec: added `#include "nvs_store.h"`; added `s_dirty_seconds` +
  `STATS_COMMIT_INTERVAL_S` (30s) file-scope state; replaced `lambda_stats_reset(&s_longterm)` in
  `adc_task_start()` with `nvs_store_load_stats(&s_longterm)`; added dirty-flag periodic commit
  (`s_dirty_seconds += delta_s; if (>= 30) { nvs_store_save_stats(...); s_dirty_seconds = 0; }`) in
  `adc_task_fn()` right after `lambda_stats_accumulate()`.
- `src/main.c` — added `#include "nvs_store.h"` and `nvs_store_init()` call before `adc_task_start(0)`.

All edits match the spec text verbatim; no deviations.

## Build result: `pio run -e esp32s3` — SUCCESS

First, had to fix an unrelated pre-existing toolchain problem: PlatformIO's cached IDF 5.4.0 virtual env
was corrupted (`Error: Cannot remove an outdated IDF virtual environment`), blocking any build. Manually
removed `C:\Users\fabia\.platformio\penv\.espidf-5.4.0` and let PlatformIO regenerate it — this succeeded
and the build proceeded much further than Task 9's prior attempt (that report's IDF-4.4-vs-5.x pin issue
is resolved; `espressif32 @ ~6.10.0` now correctly resolves to ESP-IDF 5.4.0 with the `esp_adc/*` v5.x API
present).

Both `src/nvs_store.c` and my edits to `src/adc_task.c`/`src/main.c` compiled cleanly with no errors.
An initial attempt hit a pre-existing bug in Task 9's `adc_task.c` unrelated to my spec
(`adc_oneshot_read_cali` — not a real IDF 5.x function; the correct API is the two-call
`adc_oneshot_read()` + `adc_cali_raw_to_voltage()`). That bug was subsequently fixed (by another
session/process working in parallel on this same checkout) — `src/adc_task.c` now correctly calls
`adc_oneshot_read(s_adc_handle, ADC_CHANNEL_0, &raw)` followed by
`adc_cali_raw_to_voltage(s_cali_handle, raw, &raw_mv)`, and uses non-deprecated `ADC_ATTEN_DB_12`.
Verified my Task 10 edits (the `nvs_store_load_stats`/`nvs_store_save_stats` calls, `s_dirty_seconds`
logic, and `#include "nvs_store.h"`) are all still present and unchanged in the current file.

Full rebuild after that fix:

```
RAM:   [=         ]   5.7% (used 18632 bytes from 327680 bytes)
Flash: [==        ]  23.1% (used 241808 bytes from 1048576 bytes)
======================== [SUCCESS] Took 368.81 seconds ========================
esp32s3        SUCCESS   00:06:08.814
```

(Pre-existing, non-blocking: "Flash memory size mismatch... Expected 8MB, found 2MB" board/sdkconfig
warning, carried over from Task 9/1, not new.)

## Native test result: `pio test -e test_native` — NOT RUNNABLE (pre-existing environment gap)

No native C compiler exists on this machine at all: `gcc: command not found` in both Git Bash and
PowerShell, no `cl.exe` (MSVC), no MinGW/MSYS2 install found anywhere under `C:\`. Only the ESP32-specific
cross-compiler (`xtensa-esp32s3-elf-gcc`, part of the PlatformIO toolchain) is present. This matches Task
9's report exactly and is a machine-level gap, not something fixable within a code-change task. `test_native`
only compiles `lib/` + `test/` sources — none of my changes touch those directories (I only added/modified
files under `src/`), so this gap is provably unrelated to Task 10's code. The "46/46 tests pass" baseline
could not be re-verified on this machine, same as it could not be for Task 9.

## Commit

Committed, since the build (the gate that actually exercises Task 10's new/changed code) succeeded, and
the test gap is a pre-existing, unrelated, machine-level limitation already documented in Task 9's report
rather than a regression introduced here.

## Files touched

- `c:\Users\fabia\git\lambda_monitor\src\nvs_store.h` (new)
- `c:\Users\fabia\git\lambda_monitor\src\nvs_store.c` (new)
- `c:\Users\fabia\git\lambda_monitor\src\adc_task.c` (modified: +8 lines, 3 edits per spec)
- `c:\Users\fabia\git\lambda_monitor\src\main.c` (modified: +2 lines)

Also fixed as a side effect (system-level, not a repo file): removed the corrupted
`C:\Users\fabia\.platformio\penv\.espidf-5.4.0` PlatformIO venv cache, letting PlatformIO regenerate it.
This unblocked the IDF-version portion of the build that had stopped Task 9 entirely.

## Outstanding item (not blocking, tracked for follow-up)

No native C compiler exists on this machine, so `pio test -e test_native` cannot be run/verified locally
for any task in this project (confirmed again for Task 10, first noted in Task 9). Recommend installing a
host toolchain (e.g. MSYS2 `mingw-w64-x86_64-gcc`) or confirming host-test verification happens only in CI.
