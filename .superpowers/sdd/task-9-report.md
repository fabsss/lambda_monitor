# Task 9 Report: ADC Sampling Task

## Status: BLOCKED

## What was created

- `src/adc_task.h` — written exactly per spec (snapshot struct + `adc_task_start`/`adc_task_get_snapshot` prototypes).
- `src/adc_task.c` — written exactly per spec (100 Hz sampling loop, fast filter → interpreter → category,
  once-per-second update of switch detector → warmup FSM → slow filter → ring buffer → stats accumulator,
  mutex-protected snapshot publish). All calls to lib/ modules (`signal_interpreter`, `mix_filter`, `warmup_fsm`,
  `switch_detector`, `ring_buffer`, `lambda_stats`) were verified against the actual headers in `lib/*/*.h` before
  writing — function signatures match exactly (no adjustments needed there).
- `src/main.c` — added `#include "adc_task.h"` and a call to `adc_task_start(0)` right after the boot printf,
  before any other code, per spec.

## Build result: `pio run -e esp32s3` — FAILED

```
Compiling .pio\build\esp32s3\src\adc_task.c.o
src/adc_task.c:12:10: fatal error: esp_adc/adc_oneshot.h: No such file or directory
 #include "esp_adc/adc_oneshot.h"
          ^~~~~~~~~~~~~~~~~~~~~~~
compilation terminated.
*** [.pio\build\esp32s3\src\adc_task.c.o] Error 1
```

Root cause: `platformio.ini` pins `platform = espressif32` unpinned, which PlatformIO resolved to
`espressif32 @ 2024.6.0`, bundling `framework-espidf @ 3.40408.0 (ESP-IDF 4.4.8)`. The
`esp_adc/adc_oneshot.h` / `esp_adc/adc_cali.h` / `esp_adc/adc_cali_scheme.h` headers and the
`adc_oneshot_*`/`adc_cali_*` API family are **ESP-IDF v5.x-only** — they do not exist in IDF 4.4, which instead
exposes the legacy `driver/adc.h` + `esp_adc_cal.h` API. Verified directly:
`C:/Users/fabia/.platformio/packages/framework-espidf/components/esp_adc` does not exist on disk;
only `components/driver/adc*.c` (legacy driver) is present.

This is a toolchain/environment version mismatch, not a bug in the code as written — `adc_task.c` was written
verbatim per the spec's mandated implementation, which explicitly requires the IDF 5.x oneshot/cali headers.
Silently rewriting to the legacy IDF 4.4 ADC API would deviate from the given spec, so I did not do that
without checking in first.

**Two possible resolutions** (need a decision, not mine to make unilaterally):
1. Pin `espressif32` platform to a release that bundles ESP-IDF 5.x (e.g. `platform = espressif32@6.x` or later,
   which ship IDF 5.1+), then re-run the build.
2. Rewrite `adc_task.c` to use the legacy IDF 4.4 API (`adc1_config_width`, `adc1_config_channel_atten`,
   `esp_adc_cal_characterize`, `esp_adc_cal_raw_to_voltage`, etc.) to match the currently pinned platform.

Also noted in passing (unrelated, pre-existing, non-blocking for this task): the build emitted
`Warning! Flash memory size mismatch detected. Expected 8MB, found 2MB!` for the `seeed_xiao_esp32s3` board
default — board flash size vs. sdkconfig mismatch, likely worth a `board_upload.flash_size` or
`sdkconfig.defaults` fix at some point but out of scope here.

## Native test result: `pio test -e test_native` — ERRORED (environment issue, pre-existing)

All 8 test binaries failed to build with `'gcc' is not recognized as an internal or external command`.
Verified this is **not caused by my changes** — `test_native` only compiles `lib/` + `test/` sources, never
`src/adc_task.c`/`src/main.c`. Root cause: no host-native C compiler is installed/on PATH on this machine.
`where gcc.exe` and a filesystem search under `C:/Users/fabia` found no `gcc.exe` anywhere; only the
ESP32-specific cross-compiler `xtensa-esp32s3-elf-gcc.exe` (under
`~/.platformio/packages/toolchain-xtensa-esp32s3/bin`) exists, plus a stray `libgcc_s_seh-1.dll` under
Git's mingw64 (no accompanying `gcc.exe`). This means the "46/46 tests pass" baseline from Tasks 1–8 could not
be re-verified on this machine at all — it likely passed previously via a different machine/CI environment
that had a native toolchain (e.g. MSYS2 mingw-w64-gcc or WSL) installed.

## Commit

**No commit was made.** Per the task instructions, commit only after both build and test verification succeed;
neither succeeded here, so committing would misrepresent an unverified/broken state.

## Files touched (uncommitted, present on disk)

- `c:\Users\fabia\git\lambda_monitor\src\adc_task.h` (new)
- `c:\Users\fabia\git\lambda_monitor\src\adc_task.c` (new)
- `c:\Users\fabia\git\lambda_monitor\src\main.c` (modified: +2 lines)

## Recommendation

Need explicit direction on:
1. Whether to bump the `espressif32` platform pin to get ESP-IDF 5.x (preferred — keeps the spec's mandated
   modern ADC API), or rewrite `adc_task.c` against the legacy IDF 4.4 `driver/adc.h`/`esp_adc_cal.h` API.
2. How to obtain/verify a native gcc toolchain on this machine for `pio test -e test_native` (e.g. install
   MSYS2 `mingw-w64-x86_64-gcc` and ensure `platform = native` picks it up, or confirm CI is the intended
   place this env is exercised and skip local verification).

Once either is resolved I can complete the build/test verification and commit.
