# Spec: Lambda Monitor for ESP32-S3 (Seeed Studio XIAO ESP32-S3)

**Source:** [KONZEPT.md](../../../KONZEPT.md) (original German concept document — authoritative for intent; this spec is the English, structured restatement used for planning)

## 1. Purpose

An ESP32-S3 application that:

- Captures an analog 0–3 V signal (externally op-amp-amplified from a narrowband/switching lambda sensor's raw signal) on a configurable ADC pin
- Converts it to an understandable "lean ↔ rich" scale
- Shows a live gauge + short-term (5 s) average while driving
- Tracks long-term statistics (min/max, time share per zone) that survive power loss
- Shows a time-domain signal view ("oscilloscope")
- Runs on switched ignition power (terminal 15) with no defined shutdown — the device can lose power at any instant without warning, and the design must be robust to that.

## 2. Hardware & Signal Acquisition

### 2.1 Analog Input

**Hardware base:** Seeed Studio **XIAO ESP32-S3** on the [LED Driver Board](https://wiki.seeedstudio.com/led_driver_board/), which already provides power supply and an input voltage converter/divider, so the externally op-amp-amplified 0–3 V sensor signal can be wired directly to a board terminal.

- **Pin recommendation:** one of pins **A0–A3** (`D0`–`D3`, i.e. `GPIO1`–`GPIO4`); default **A0 (`D0` / `GPIO1`)**, since it is the pin least likely to be reserved for other onboard functions (`D4`/`D5` = `GPIO5`/`GPIO6` are the standard I²C pins SDA/SCL; `D8`–`D10` are the standard SPI pins — both potentially used by the LED Driver Board for onboard peripherals). The ADC pin remains **configurable** (selectable from `A0`–`A3`, optionally `A4`/`A5` if I²C is unused on the board).
- **Critical ESP32-S3 constraint:** `GPIO1`–`GPIO10` (i.e. `A0`–`A5` and `D8`–`D10`) are on **ADC1**; `GPIO11`–`GPIO20` are on **ADC2**. Because **ADC2 shares hardware with the WiFi driver** and is unreliable/blocked while the SoftAP is running (see §7), the input pin **must** be an **ADC1 pin** — `A0`–`A3` satisfy this and are the safe choice.
- ⚠️ **Unverified:** the exact LED Driver Board routing (which physical signal/terminal input maps to which XIAO GPIO) could not be confirmed against Seeed's wiki in the environment that authored KONZEPT.md. Verify against the schematic/board silkscreen before wiring that the used input actually routes to an `A0`–`A3` pin; if not, select a different ADC1-capable pin (`GPIO1`–`GPIO10`) in configuration.
- Use **ADC calibration** (`esp_adc/adc_cali`) to compensate component tolerances; ADC values are always converted to mV internally and never processed as raw counts.

### 2.2 Configurable Voltage Range

Because the external op-amp gain varies by wiring/sensor, the valid voltage range is not hardcoded but stored as calibration values:

| Parameter | Meaning | Default |
|---|---|---|
| `u_min` | Voltage at "very lean" (sensor minimum) | 0.0 V |
| `u_max` | Voltage at "very rich" (sensor maximum) | 3.0 V |
| `u_lambda1` | Voltage at the λ = 1 switching point | 1.5 V |
| `deadband` | Tolerance band around `u_lambda1` still counted as "λ = 1" | ±0.15 V |

These four values are editable via the web UI (config page, §8.4) and persisted to flash (NVS) with the rest of the configuration.

### 2.3 Averaging / Noise Suppression

Two-stage filter design, since two different time constants are needed (fast live display vs. slower driving average):

1. **Sampling:** ADC sampled at a fixed rate (e.g. 100 Hz)
2. **Fast denoising:** moving average / small median filter over ~5–10 samples immediately after sampling, to suppress ADC/ignition noise without swallowing the sensor's switching dynamics (switching frequency, see §3.3)
3. **Slow display average:** separate moving average over the last 5 s (ring-buffer- or EMA-based) for the "average over a longer window" required on Screen 1

Both averages run in parallel and independently, so the fast display (needle) is not made sluggish by the 5 s smoothing.

## 3. Signal Interpretation

### 3.1 Switching Sensor Characteristics (drives the scaling design)

A lambda **switching sensor** (zirconia sensor, not a wideband sensor) does not produce a signal linearly proportional to λ. It flips abruptly between a low voltage (lean) and a high voltage (rich) around the stoichiometric point λ = 1 — the curve is extremely steep in between, and outside that narrow band the sensor gives **no** reliable quantitative statement about the exact λ value.

**Design decision:** instead of faking a pseudo-precise λ number (which would be technically wrong), compute a **"mixture index"** on a normalized scale from **-100 (very lean)** through **0 (λ = 1)** to **+100 (very rich)** — linearly interpolated between the calibration points `u_min → -100`, `u_lambda1 → 0`, `u_max → +100`. A textual category is also derived:

| Mixture index | Category |
|---|---|
| -100 … -60 | very lean |
| -60 … -20 | lean |
| -20 … +20 | λ ≈ 1 (target zone, `deadband`) |
| +20 … +60 | rich |
| +60 … +100 | very rich |

Category thresholds are also configurable (same config page). A real λ approximation could later be added if the sensor is swapped for a wideband unit — the architecture (§9) provides a swappable "signal interpreter" module for this.

### 3.2 Sensor Readiness (Warm-up Phase)

Switching sensors only produce a valid signal above ~300 °C. A "stuck" signal is **not** necessarily just "sensor still cold" — it can equally mean the engine is genuinely running lean or rich for an extended period. These two cases must not be confused, or a real persistent fault would be incorrectly masked as "sensor not ready."

**Design: warm-up phase only as a state immediately after power-on, time-bounded, never re-entered:**

A small state machine with exactly two states per operating cycle (ignition on → off):

```
        Power-On
           │
           ▼
     ┌───────────┐   switching activity detected   ┌───────────┐
     │  WARMUP   │ ────────────────────────────────►│ OPERATING │
     └───────────┘   OR timeout t_warmup             └───────────┘
           │                                               │
           │ (max. t_warmup, configurable)                 │ stays until
           └───────────────────────────────────────────────┘ power-off
```

- **`WARMUP`** applies **only** immediately after power-on (system start). Transition to `OPERATING` happens as soon as **either**:
  - the first plausible switching edge is detected (signal crosses `u_lambda1` with sufficient amplitude, see §3.3), **or**
  - a configurable timeout `t_warmup` elapses (default **90 s** — heated switching sensors, standard since the 1990s, typically reach operating temperature after ~20–60 s; older unheated sensors may need 1–3 minutes depending on load/ambient temperature. 90 s provides safety margin over the typical case and remains editable via the config page for a specific sensor/vehicle).
- **`OPERATING`** is **never exited again for the rest of the operating cycle** — a later sticking signal is always treated as a regular measurement from here on and flows normally into the statistics ("too lean"/"too rich" over time). There is **no** re-interpretation as "sensor not ready."
- While `WARMUP`, Screen 1 shows the status **"Sensor warming up…"**, and mixture-index/statistics accumulation is paused during this phase (see §4.1), since the values are not meaningful yet.

**Additional feature (additive, not a replacement):** since a persistent stuck signal after warm-up is itself a *reportable event* (open control loop, sensor fault, or a genuine persistent lean/rich fault), a **separate, informational notice** is shown that does **not** affect statistics: e.g. "⚠ No sensor switching for 5 min" on Screen 1, once no switching edge has been detected for longer than `t_warmup` since entering `OPERATING`. This is purely additive (see also §3.3/§8.1) and does not replace the normal lean/λ1/rich evaluation.

### 3.3 Switching Frequency as a Secondary Metric

Sensor switching frequency (number of zero-crossings around `u_lambda1` per minute) is in practice a good indicator of closed-loop mixture control dynamics. This frequency is additionally logged (see §4) and shown on Screen 1 — a sudden freeze of switching frequency indicates e.g. an open control loop or sensor failure.

## 4. Statistics & Long-Term Storage

### 4.1 Runtime Data (RAM)

- **Ring buffer** of fixed size for the live curve (Screen 2), e.g. 300–600 points, decimated/downsampled to a configurable time window (e.g. 60 s) — buffer size is independent of sample rate and constant, and it **never overflows** (oldest point is overwritten).
- **Accumulators** for statistics: time "lean"/"λ=1"/"rich" **and** time "warm-up" in seconds (`uint32_t`, lasts >100 years at 1 Hz increment → practically no overflow risk), min/max mixture index with timestamps, switching frequency (moving average). Warm-up time is counted as its own seconds value (see §3.2) so it is visible as its own share in the evaluation (§8.1) without being mixed into "lean"/"rich".
- Statistics are kept at **two levels**:
  - **Session statistics:** since the last power-on (ignition on)
  - **Long-term statistics:** cumulative across all sessions (persisted)

### 4.2 Flash Persistence (NVS, not "EEPROM")

The ESP32-S3 has no real EEPROM; it uses the **NVS driver** (Non-Volatile Storage, flash-based, ESP-IDF) which already includes **wear leveling** and **power-loss safety** (atomic writes via a log-structured format). This is the correct choice to structurally satisfy "no corrupted state after power loss" and "no memory overflow":

- Only the **aggregated long-term statistics** (fixed, small struct size) are persisted — **not** the time series. Flash usage is therefore constant and never grows unbounded (no "endless log" that could eventually fill up).
- **Periodic writes** instead of write-on-every-change: NVS commit only every e.g. 30–60 s ("dirty flag" + timer) plus on significant events (e.g. new min/max value). This drastically reduces flash wear (flash typically tolerates ~100,000 write cycles per physical cell; NVS additionally spreads writes across sectors).
- **Additional safeguard** beyond NVS: the struct has a `struct_version` field and a CRC32 checksum; the checksum is validated at boot. On mismatch (e.g. power loss exactly during a write), the system falls back to a default/zero state instead of continuing to compute with corrupted values.
- Since a power loss could occur during the write window (a few ms, NVS commit) anyway: NVS itself remains consistent during this (old or new version, never a mixed state) — this is the core advantage over a naive custom "EEPROM emulation."

### 4.3 Resetting Long-Term Storage

- Button on Screen 1 ("Reset long-term statistics"), with a confirmation dialog (destructive action)
- Resets the long-term struct in RAM **and** in NVS
- Session statistics are independent of this and are always "since power-on" by definition

### 4.4 Data Structure (conceptual)

```c
typedef struct __attribute__((packed)) {
    uint16_t struct_version;
    uint32_t t_warmup_s;      // time in warm-up phase [s]
    uint32_t t_lean_s;        // time in "lean" zone [s]
    uint32_t t_lambda1_s;     // time in target zone [s]
    uint32_t t_rich_s;        // time in "rich" zone [s]
    int16_t  index_min;       // smallest mixture index ever measured
    int16_t  index_max;       // largest mixture index ever measured
    uint32_t total_runtime_s; // total runtime since first use
    uint32_t crc32;           // checksum over the fields above
} lambda_longterm_stats_t;
```

## 5. Avoiding Memory Overflow (Summary)

| Memory | Measure |
|---|---|
| RAM ring buffer (live curve) | fixed size, oldest values overwritten (FIFO) |
| RAM accumulators | sufficiently wide integer types (uint32/int16), no unbounded array |
| Flash / NVS | only an aggregated fixed-size struct is written (no raw data log), periodic instead of continuous writes, NVS's own wear leveling |
| Configuration | also a fixed, small struct in NVS |

## 6. Behavior Around Ignition Power

- **Power-on:** peripheral init, load long-term statistics + configuration from NVS (with CRC check, fallback to defaults), start WiFi access point and HTTP server, session statistics initialized to 0
- **Running operation:** as above, periodic NVS commits
- **Power-off (unannounced power loss):** since no clean shutdown signal exists (ignition power can drop hard at any time), **no** special shutdown handling is assumed — robustness instead comes from every persisted state being **valid at all times** (see §4.2: atomic NVS writes, CRC protection, no multi-step/dependent writes that are only consistent "together")
- **Optional (future):** use the ESP32-S3's internal **brown-out detector** to trigger an immediate, out-of-cycle NVS commit on detected voltage sag — not required per §4.2, but an optimization to keep values as current as possible

## 7. Network & Web UI

- ESP32-S3 starts its own **WiFi access point (SoftAP)** with configurable SSID/password; no internet access needed
- Optional: captive-portal redirect so phones auto-open the UI on connect
- **HTTP server** (`esp_http_server`) serves the static web UI (HTML/CSS/JS embedded **in the firmware image** as `const char[]`, not on a separate filesystem like LittleFS/SPIFFS) — this avoids an additional, potentially corruptible filesystem for pure UI assets; only actual measurement values/configuration live in NVS
- **Live data push:** WebSocket endpoint (supported by `esp_http_server`) for Screen 1 (current value, ~5–10 Hz) and Screen 2 (curve points); REST endpoints (`GET /api/stats`, `POST /api/reset`, `GET/POST /api/config`) for statistics retrieval, reset, and configuration

## 8. Screens

### 8.1 Screen 1 — Live Value

- **Needle/arc gauge** (SVG, tachometer-style) over the mixture index (-100…+100), color-zoned **lean = red / target zone (λ = 1) = green / rich = blue** — readable at a glance while driving (same color coding used consistently on the time-view screen §8.2 and the percentage bar below)
- **5 s average** as a second, more subdued needle/marker on the same scale (direct visual comparison of instantaneous value vs. trend)
- **Additional elements** for this screen:
  - Status indicator "Sensor ready / not ready" (§3.2)
  - Current switching frequency [switches/min] as a control-loop indicator (§3.3)
  - Session statistics: time shares **warm-up/lean/λ=1/rich** as a stacked percentage bar, min/max since power-on
  - Long-term statistics (persisted): same metrics cumulative, plus total runtime, with a "Reset" button (§4.3)
  - **Percentage bar rendering note:** the data structure stores only seconds values (see §4.4, incl. `t_warmup_s`) — conversion to percent (`share = t_x_s / (t_warmup_s + t_lean_s + t_lambda1_s + t_rich_s) * 100`) happens **exclusively in the frontend** (JavaScript), not on the ESP32. The bar is rendered as a four-part stacked bar (100% = total runtime): warm-up (gray) / lean (red) / λ = 1 (green) / rich (blue) — same color coding as the gauge
  - WiFi/connection status, uptime
  - Warning if the signal has been stuck unusually long in "very lean"/"very rich" (possible sensor fault or engine problem)

### 8.2 Screen 2 — Time View ("Oscilloscope")

- Line chart (canvas-based, e.g. `uPlot` — lightweight enough for live rendering in-browser) of the mixture index over time
- Selectable time window (e.g. 10 s / 30 s / 60 s), fed from the RAM ring buffer (§4.1)
- Horizontal reference line at "λ = 1" (green) plus marking of the `deadband` zone; curve/background optionally colored in the §8.1 zone colors (lean = red, λ = 1 = green, rich = blue)
- Pause/freeze button to hold a moment for closer analysis

### 8.3 Navigation

Simple tab/button navigation between Screen 1 and 2 on the same single-page web app (no reload needed, WebSocket connection stays active).

### 8.4 Configuration Page

Third, simple area (e.g. "Settings" tab):

- ADC pin selection
- Voltage calibration points `u_min` / `u_max` / `u_lambda1` / `deadband` (manual **or** via the calibration wizard, §8.5)
- Category thresholds (§3.1)
- Averaging window lengths (fast/slow)
- Warm-up timeout `t_warmup` (§3.2)
- Access point WiFi SSID/password
- Saving writes an NVS commit **immediately** (not periodically), since config changes are rare and deliberately triggered

### 8.5 Calibration Wizard (fixed part of the config page)

Instead of only manual number entry, a guided wizard is provided with two modes:

**a) Guided manual calibration (base variant, always available):**
The wizard shows the current live voltage and offers three buttons:
1. "Run engine at idle with stable mixture (λ ≈ 1) → *Take current value as the λ=1 point*"
2. "Drive to the **lean** extreme (e.g. briefly induce a vacuum leak/overrun) → *take as `u_min`*"
3. "Drive to the **rich** extreme (e.g. briefly choke/full-load enrichment) → *take as `u_max`*"

Each button captures the current filtered (5 s average) measurement into the corresponding calibration field; `deadband` gets a default suggestion derived from it (e.g. 10% of the distance to `u_min`/`u_max`) and remains manually editable.

**b) Automatic calibration ("Auto-Calibrate" button):** With the engine running in closed-loop control, the sensor already oscillates continuously between its two extremes. The wizard observes the signal for a configurable time window (default 30 s) and automatically derives:
- `u_min` ≈ 5th percentile of observed values
- `u_max` ≈ 95th percentile of observed values
- `u_lambda1` ≈ midpoint between the two most common switching edges
- `deadband` ≈ spread of switching points around `u_lambda1`

The automatically derived values are shown for review before being applied (not auto-saved), so obvious measurement errors (e.g. engine was idle/off during measurement) can be manually corrected or discarded.

### 8.6 OTA Firmware Update via Web UI

A direct firmware update feature is a fixed part of the config page (not a future item):

- Dedicated "Firmware Update" tab with file upload (`.bin`) via `POST /api/ota`, progress indicator during upload
- Implemented via ESP-IDF's own OTA functionality (`esp_https_ota`/`esp_ota_ops`) with **two OTA app partitions** (`ota_0`/`ota_1`) plus `otadata` in the partition table, so a working previous image is always retained
- **Rollback protection:** the new image is marked "pending verify" after reboot; only after a successful self-test (boot completed, WiFi AP + HTTP server running, config/NVS readable) is it confirmed valid via `esp_ota_mark_app_valid_cancel_rollback()`. If the self-test fails or the new image doesn't boot cleanly, the ESP32-S3 automatically falls back to the previous working image on the next start (ESP-IDF bootloader feature).
- Since the access point is only reachable locally anyway (no internet), a simple safeguard against accidental/unauthorized flashing is added: confirmation dialog in the frontend + optional password prompt before upload
- Persisted statistics/config in NVS is unaffected by an OTA update (separate partitions) — an update resets neither long-term statistics nor calibration

## 9. Software Architecture (Modules/Tasks)

FreeRTOS tasks with clear responsibilities, communicating via queues:

```
[ADC Sampling Task] --(raw mV)--> [Filter/Interpreter] --(index, category)-->
    ├──> [Statistics Task] --(periodic)--> [NVS Persistence]
    ├──> [RAM Ring Buffer] --(for Screen 2)
    └──> [WebSocket Broadcast Task] --> Web Clients
[HTTP/WS Server Task] <--(REST: /api/stats, /api/reset, /api/config)--> [Config/Stats Module]
[HTTP Server Task] <--(POST /api/ota, firmware binary)--> [OTA Task] --> [esp_ota_ops, OTA partitions]
```

- **Signal interpreter** as a swappable module (interface), so e.g. a wideband-sensor variant with real λ calculation can later be added without touching the rest of the app (see §3.1)
- Clear separation: acquisition/interpretation (hard real-time requirements, low latency) vs. persistence/web (timing-uncritical)

## 10. Technology Stack

| Area | Choice | Rationale |
|---|---|---|
| Build system | **PlatformIO**, `framework = espidf` (not Arduino core, not raw `idf.py`) | user has PlatformIO Core 6.1.18 installed locally with `espressif32` platform, `framework-espidf@5.3.2`, and `toolchain-xtensa-esp32s3` already present — no separate ESP-IDF install needed; PlatformIO drives the same underlying ESP-IDF build (CMake+ninja) with a simpler CLI/dependency/upload workflow than bare `idf.py`. Arduino core was considered and rejected: it lacks built-in WebSocket support in its HTTP server, wraps NVS behind the much more limited `Preferences.h` API (incompatible with the checksummed-struct design in §4.2/§4.4), and has a weaker OTA rollback story than raw `esp_ota_ops` (§8.6) |
| Framework | ESP-IDF (native, FreeRTOS) APIs, via PlatformIO | direct access to ADC calibration, NVS, `esp_http_server` incl. WebSocket, lower overhead |
| Persistence | NVS (`nvs_flash`) | wear leveling & power-loss safety built in (see §4.2) |
| Web server | `esp_http_server` with WS support | no extra library needed, well tested, async |
| Frontend | Vanilla HTML/CSS/JS + `uPlot` (chart) + inline SVG (gauge) | no build pipeline needed, fully embeddable in firmware, small enough for flash |
| API data format | JSON (`cJSON`, included in ESP-IDF) | simple browser interoperability |
| OTA | `esp_https_ota`/`esp_ota_ops`, partition table with `ota_0`/`ota_1`/`otadata` | built-in rollback protection (see §8.6), no custom bootloader code needed |
| Host unit tests | PlatformIO `native` platform (`env:test_native` in `platformio.ini`) running Unity | lets §12's hardware-agnostic modules build and run as plain C on the host via `pio test -e test_native`, no ESP32 attached, no separately installed system compiler required — PlatformIO resolves its own host toolchain |
| Target hardware | Seeed Studio **XIAO ESP32-S3** + LED Driver Board | S3 over C3: more ADC1 pins (GPIO1–10), proven bring-up board with input voltage converter (see §2.1) |

## 11. Open Items / Future Work

Explicitly out of scope for the initial implementation plan:

- Export of long-term statistics (CSV download over HTTP)
- mDNS (`lambda.local`) instead of a fixed IP on the hotspot
- Multiple simultaneous client connections (WebSocket broadcast is already designed to support this)
- Physical verification of the LED Driver Board pin mapping against the schematic (see note in §2.1) and adjustment of the pin recommendation if needed

## 12. Testing Strategy

The hardware-agnostic core logic is designed to be unit-testable on the host (no ESP32 required), decided as follows:

- **Host-testable (pure C, no ESP-IDF dependency, built and run via PlatformIO's `native` platform + Unity: `pio test -e test_native`):**
  - Signal interpreter: mV → mixture index, category classification (§3.1)
  - Warm-up state machine (§3.2)
  - Switching-frequency detection (§3.3)
  - Fast/slow averaging filters (§2.3)
  - Ring buffer for the live curve (§4.1)
  - Statistics accumulators, percentage-share math is frontend-only per §8.1 so no host test needed there
  - CRC32 + struct versioning validation logic (§4.2)
  - Calibration wizard math (percentile/midpoint derivation, §8.5b)
- **Hardware-only (requires a PlatformIO `env:esp32s3` build + flashing to XIAO ESP32-S3 via `pio run -e esp32s3 -t upload`, verified manually or via an on-target Unity test — no automated host test):**
  - ADC sampling + calibration (`esp_adc/adc_cali`)
  - NVS read/write, wear leveling behavior, power-loss survival
  - WiFi SoftAP + HTTP/WebSocket server
  - OTA update + rollback
  - Frontend rendering in an actual browser against the live device

This split determines task structure in the implementation plan: core logic gets TDD unit-test tasks; HAL-dependent modules get an implementation task plus an explicit manual hardware-verification checklist instead of an automated test.
