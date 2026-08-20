# Task 10 Report: NVS Persistence (Long-term Stats + Config)

## Status: BLOCKED

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

## Build result: `pio run -e esp32s3` — FAILED (pre-existing bug in Task 9's code, not mine)

First, had to fix an unrelated pre-existing toolchain problem: PlatformIO's cached IDF 5.4.0 virtual env
was corrupted (`Error: Cannot remove an outdated IDF virtual environment`), blocking any build. Manually
removed `C:\Users\fabia\.platformio\penv\.espidf-5.4.0` and let PlatformIO regenerate it — this succeeded
and the build proceeded much further than Task 9's prior attempt (that report's IDF-4.4-vs-5.x pin issue
is resolved; `espressif32 @ ~6.10.0` now correctly resolves to ESP-IDF 5.4.0 with the `esp_adc/*` v5.x API
present).

Both `src/nvs_store.c` and my edits to `src/adc_task.c`/`src/main.c` compiled cleanly with no errors. The
build then failed on a line in `adc_task.c` that Task 10's spec did not ask me to touch:

```
src/adc_task.c:43:9: error: implicit declaration of function 'adc_oneshot_read_cali'; did you mean 'adc_oneshot_read'? [-Wimplicit-function-declaration]
   43 |         adc_oneshot_read_cali(s_adc_handle, ADC_CHANNEL_0, &raw_mv);
```

Verified: `adc_oneshot_read_cali` does not exist anywhere in
`C:/Users/fabia/.platformio/packages/framework-espidf/components/esp_adc/include/esp_adc/*.h`. The IDF 5.x
API is two separate calls — `adc_oneshot_read(handle, channel, &raw)` followed by
`adc_cali_raw_to_voltage(cali_handle, raw, &voltage)` — not a combined `_read_cali` function. This is a
pre-existing bug in Task 9's `adc_task.c` (written by the Task 9 agent, unrelated to anything in the
Task 10 spec I was given, which only lists 3 additive edits to that file). Task 10's instructions say to
copy the two NVS files exactly and make specific listed edits elsewhere — rewriting Task 9's ADC read call
is out of scope and not something I should do unilaterally.

(Also emitted, non-blocking: `ADC_ATTEN_DB_11` deprecation warnings in `adc_task_start()`, and the
pre-existing "Flash memory size mismatch... Expected 8MB, found 2MB" board/sdkconfig warning — both
carried over from Task 9, not new.)

## Native test result: `pio test -e test_native` — NOT RUN (pre-existing environment gap)

No native `gcc` exists on this machine (`gcc: command not found` in both Git Bash and PowerShell; only
`xtensa-esp32s3-elf-gcc` under the PlatformIO ESP32 toolchain is present). This matches Task 9's report
exactly. `test_native` only compiles `lib/` + `test/` sources — my changes are entirely in `src/`, so this
gap is unrelated to Task 10's code, but it means the "46/46 tests pass" claim could not be re-verified on
this machine.

## Commit

**No commit was made.** Build did not succeed (blocked by Task 9's `adc_oneshot_read_cali` bug) and native
tests could not be run (no gcc), so per the task instructions ("Run `pio run -e esp32s3` — should succeed"
/ "confirm 46/46 tests pass") neither verification gate is met. Committing would misrepresent an unverified
state.

## Files touched (uncommitted, present on disk)

- `c:\Users\fabia\git\lambda_monitor\src\nvs_store.h` (new)
- `c:\Users\fabia\git\lambda_monitor\src\nvs_store.c` (new)
- `c:\Users\fabia\git\lambda_monitor\src\adc_task.c` (modified: +8 lines, 3 edits per spec)
- `c:\Users\fabia\git\lambda_monitor\src\main.c` (modified: +2 lines)

Also fixed as a side effect (system-level, not a repo file): removed the corrupted
`C:\Users\fabia\.platformio\penv\.espidf-5.4.0` PlatformIO venv cache, letting PlatformIO regenerate it.
This unblocked the IDF-version portion of the build that had stopped Task 9 entirely; the build now
progresses past dependency resolution and compiles all of Task 10's own code successfully.

## Recommendation

Need a decision on `src/adc_task.c:43` (Task 9's code, not Task 10's):
1. Fix `adc_oneshot_read_cali(s_adc_handle, ADC_CHANNEL_0, &raw_mv)` to the correct two-call IDF 5.x
   sequence: `adc_oneshot_read(s_adc_handle, ADC_CHANNEL_0, &raw)` then
   `adc_cali_raw_to_voltage(s_cali_handle, raw, &raw_mv)`. This is likely a one-line-becomes-two-line fix,
   but it's a correction to Task 9's deliverable, so I did not make it unilaterally under a Task 10 spec
   that didn't ask for it.
2. Separately, get a native gcc toolchain on this machine (e.g. MSYS2 `mingw-w64-x86_64-gcc`) or confirm
   host-test verification is expected to happen only in CI, so `pio test -e test_native` can actually be
   run here.

Once either the ADC read call is fixed (by me or by direction to do so) or the task is scoped to skip
`pio run` verification, I can complete verification and commit.
