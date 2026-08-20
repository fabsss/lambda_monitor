# SDD ledger — plan: docs/superpowers/plans/2026-08-20-lambda-monitor.md

**Conflict scan:** Clean. 14 tasks, clear DAG: scaffolding → host-testable core → hardware integration. No conflicts found.

**Rulings at pre-flight:** None.

---

## Task Progress

- [x] Task 1: PlatformIO project skeleton (f311d44)
- [x] Task 2: Signal interpreter (mV → index + category) (9e4c03d)
- [x] Task 3: Warm-up state machine (301e020)
- [x] Task 4: Switching-frequency detector (bb67c9a)
- [x] Task 5: Fast/slow averaging filters (9dd2ef6)
- [x] Task 6: Fixed-size ring buffer (2f2a17f)
- [x] Task 7: CRC32 + statistics struct (541459a)
- [x] Task 8: Calibration wizard math (ba71057)
- [x] Task 9: ADC sampling task (4cc10a5) — completed, build/tests verified
- [x] Task 10: NVS persistence (1799b0f) — build verified (`pio run -e esp32s3` SUCCESS); native tests
      not runnable on this machine (no host gcc toolchain, pre-existing gap noted in Task 9)
- [x] Task 11: WiFi SoftAP + HTTP/WebSocket server (dfbf367)
- [x] Task 12: Frontend (gauge, chart, settings) (e867641)
- [x] Task 13: Screen 2 chart (uPlot) (cddb99b)
- [x] Task 14: OTA firmware update (f7da428)

**Phase B Complete:** All 46 host tests pass. Core logic ready for hardware integration.
**Phase C Complete:** All 14 tasks implemented. Firmware builds successfully (Flash 28.7%).

---

## Final Whole-Branch Review

**Verdict: Do not merge as-is.**

**5 Critical Blockers Identified:**
1. **B1** — CRC never written during stats save; long-term stats destroyed every reboot (B1)
2. **B2** — Saved calibration config never applied; ADC always uses defaults
3. **B3** — Config POST sends strings, backend parses as JSON numbers → all fields become 0
4. **B4** — OTA endpoint unauthenticated; no image validation; unchecked writes
5. **B5** — Rollback protection disabled; image marked valid unconditionally

**Major Spec Gaps:**
- H1: Session vs. long-term stats model missing (only one accumulator)
- H2: WebSocket never broadcasts; live display inert (gauge, status, wizard frozen)
- H4: Calibration wizard writes wrong-unit data (index as mV)
- H5: Auto-calibration dead code; no endpoint/UI/buffer management

**Strengths:**
- Core logic (`lib/`, Phase B) is sound: 46 tests pass, CRC32 correct, FSM correct, signal chain correct
- ADC1 constraint respected, calibration API present, OTA framework in place
- Clean architecture separation (host-testable core vs. hardware-dependent integration)

**Root Cause:**
Spec §12 mandates "verified by flashing to real hardware" for Phase C. The subagent-driven workflow marked tasks complete based on build + host tests (valid for Phase B), but on-hardware acceptance criteria were never invoked. All blockers would be immediately obvious on a real device.

**Recommendation:**
Fix B1–B5 before merge (correctness/data-loss/security). Triage H1–H5 and M1–M9 as follow-up tasks or acknowledged spec descopes. Re-run whole-branch review after fixes.

---
