# Tasks 3–8: Core Logic Modules Implementation Report

**Status:** DONE

**Commit Range:** 9e4c03d..ba71057 (6 new commits, Task 3 through Task 8)

**Commit Hashes (in order):**
1. Task 3 (warmup_fsm): `301e020`
2. Task 4 (switch_detector): `bb67c9a`
3. Task 5 (mix_filter): `9dd2ef6`
4. Task 6 (ring_buffer): `2f2a17f`
5. Task 7 (crc32 + lambda_stats): `541459a`
6. Task 8 (calib_wizard): `ba71057`

## Important environment fix (blocking issue found and resolved)

Before any of these tasks could be verified, two environment problems were discovered and fixed:

1. **No native C compiler was installed.** `pio test -e test_native` could not run at all (Task 2's report had only "manually traced" its tests for this reason). Installed `gcc` 15.2.0 via `scoop install gcc` (nuwen MinGW distribution) to make host testing actually possible.
2. **Incorrect PlatformIO test directory layout.** The existing `test/test_native/` held multiple `test_*.c` files directly. PlatformIO's native test runner compiles and links every `.c` file under one test directory into a single binary, so each file's own `setUp`/`tearDown`/`main` collided ("multiple definition" link errors) as soon as a second test file was added. Fixed by moving each test file into its own subdirectory (`test/<test_name>/test_*.c`), PlatformIO's documented one-test-per-directory convention. This was folded into the Task 3 commit since it was required before Task 3's own test could run, and it also caused Task 2's 9 tests to actually execute (and pass) for the first time.

All 6 tasks below were implemented with real `pio test -e test_native` runs (build + execute), not manual tracing — every test failure encountered during development is noted per task.

## Task 3: Warm-up State Machine

- `lib/warmup_fsm/warmup_fsm.h`, `lib/warmup_fsm/warmup_fsm.c`
- `test/test_warmup_fsm/test_warmup_fsm.c` — 5 tests, all passed on first run.
- Two-state FSM (WARMUP → OPERATING), transitions on switching-edge detection or a configurable timeout (90s default per spec §3.2); OPERATING is terminal (tick() becomes a no-op).

## Task 4: Switching-Frequency Detector

- `lib/switch_detector/switch_detector.h`, `lib/switch_detector/switch_detector.c`
- `test/test_switch_detector/test_switch_detector.c` — 5 tests.
- One test (`test_seconds_since_last_edge_resets_on_edge`) failed on first run due to an arithmetic error in my own test expectation (expected 5, actual/correct value 6, since the very first sample already contributes its own delta_s before any edge can occur). Corrected the test assertion; re-ran and all 5 passed.
- Tracks zero-crossings of the mixture index over a rolling 60s window (switches_per_min) plus seconds since the last edge.

## Task 5: Fast/Slow Averaging Filters

- `lib/mix_filter/mix_filter.h`, `lib/mix_filter/mix_filter.c`
- `test/test_mix_filter/test_mix_filter.c` — 5 tests, all passed on first run.
- `fast_filter_t`: 8-sample circular moving average for live display. `slow_filter_t`: configurable time-windowed average (5s default) for trend display, publishing `last_avg` each time the window elapses.

## Task 6: Fixed-Size Ring Buffer

- `lib/ring_buffer/ring_buffer.h`, `lib/ring_buffer/ring_buffer.c`
- `test/test_ring_buffer/test_ring_buffer.c` — 5 tests, all passed on first run.
- 600-point FIFO ring buffer (index_value + timestamp pairs) for the Screen 2 oscilloscope curve. Fixed capacity, overwrites oldest entry when full; verified FIFO ordering, overflow behavior, and out-of-range access.

## Task 7: CRC32 + Versioned Statistics Struct

- `lib/lambda_stats/crc32.h`, `lib/lambda_stats/crc32.c`
- `lib/lambda_stats/lambda_stats.h`, `lib/lambda_stats/lambda_stats.c`
- `test/test_crc32/test_crc32.c` — 3 tests, all passed on first run (including the standard CRC-32/ISO-HDLC check value 0xCBF43926 for "123456789").
- `test/test_lambda_stats/test_lambda_stats.c` — 7 tests (one more than the spec's suggested 6, added an explicit version-mismatch-only case), all passed on first run.
- Bitwise CRC-32 (IEEE 802.3, polynomial 0xEDB88320, no lookup table). `lambda_longterm_stats_t` is a packed, versioned struct (spec §4.4) tracking time in warmup/lean/lambda1/rich buckets, index min/max, and total runtime. `index_min`/`index_max` are intentionally NOT updated while `in_warmup` is true (spec §3.2); `total_runtime_s` and `t_warmup_s` always accumulate. `lambda_stats_validate` checks `struct_version` before recomputing the CRC.
- This module also exercised cross-library dependencies correctly under PlatformIO's LDF (`lambda_stats.c` depends on both `crc32.h` in the same lib dir and `signal_interpreter.h` from a separate lib) — resolved automatically, no build config changes needed.

## Task 8: Calibration Wizard Math

- `lib/calib_wizard/calib_wizard.h`, `lib/calib_wizard/calib_wizard.c`
- `test/test_calib_wizard/test_calib_wizard.c` — 7 tests, all passed on first run.
- `calib_sort_i32`: insertion sort. `calib_percentile`: nearest-rank percentile over a pre-sorted array. `calib_auto_derive`: computes `u_min_mv`/`u_max_mv` from the 5th/95th percentiles, `u_lambda1_mv` as their midpoint, and `deadband_mv` as 5% of the percentile span (matching the default calibration's 150/3000 mV ratio, per spec §8.5b). Sorts a local variable-length-array copy so the caller's input is never mutated (verified explicitly by test).

## Final Verification

Full suite run after Task 8 (`pio test -e test_native`, gcc 15.2.0 via scoop):

```
Environment    Test                     Status    Duration
-------------  -----------------------  --------  ------------
test_native    test_calib_wizard        PASSED    00:00:01.347
test_native    test_crc32               PASSED    00:00:01.663
test_native    test_lambda_stats        PASSED    00:00:02.009
test_native    test_mix_filter          PASSED    00:00:02.657
test_native    test_ring_buffer         PASSED    00:00:02.445
test_native    test_signal_interpreter  PASSED    00:00:02.038
test_native    test_switch_detector     PASSED    00:00:02.192
test_native    test_warmup_fsm          PASSED    00:00:02.260
================= 46 test cases: 46 succeeded in 00:00:16.611 =================
```

All 8 test suites (Task 2's carried-over `test_signal_interpreter` plus Tasks 3–8) pass — 46 total test cases, 0 failures.

## Concerns / Notes for follow-up

1. **gcc install is machine-local, not committed.** `pio test -e test_native` requires `gcc` on PATH; this environment did not have one and it was installed via `scoop install gcc`. Any other machine (or CI) running this suite will need a native C toolchain available the same way. Worth documenting in the repo (e.g. README or CI config) so this isn't rediscovered.
2. **Test directory layout fix affects Task 2's file location.** `test/test_native/test_signal_interpreter.c` was moved to `test/test_signal_interpreter/test_signal_interpreter.c`. This is a rename only — no behavior change — but it means any documentation referencing the old path (e.g. Task 2's report) is now stale on that detail.
3. No other blockers. All six modules are pure C with zero ESP-IDF dependencies, as required, and fully unit-tested on the host `native` platform.
