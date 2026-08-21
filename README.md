# lambda_monitor

An ESP32-S3 firmware that reads a lambda (oxygen) step sensor's analog
output, interprets it as a mixture index, and serves a live dashboard over
its own WiFi access point — no app, no cloud, no internet connection needed.
Built for monitoring closed-loop mixture control while driving.

## Features

- **Live gauge** — fast (~80ms) and slow (5s) needle, current voltage,
  switching frequency, and a 2s rolling control-quality average (should
  hover near 0/center even during normal lean↔rich dither; a sustained
  offset signals a real mixture bias).
- **Session & long-term statistics** — time spent in each of six buckets
  (warmup, very lean, lean, λ=1, rich, very rich), survives power loss via
  flash (NVS) storage, resettable independently of session stats.
- **Chart / oscilloscope view** — voltage over the last 10/30/60s, with
  color-coded lean/λ=1/rich zones based on the live calibration; freezable.
- **Calibration** — manual entry, a guided wizard (tap a button to capture
  the current live voltage as `u_min`/`u_lambda1`/`u_max`), or automatic
  calibration (collects 100 samples while driving through the full mixture
  range, derives calibration from percentiles).
- **OTA firmware updates** over the web UI — no cable needed after the
  first flash.
- **Screen wake lock** — keeps the phone display on while the page is open
  (browser-dependent; typically requires the device to be reachable over a
  secure context).
- Firmware version and build time are shown under Settings, sourced from
  the actual git tag the binary was built from (not a hand-maintained
  string) — see `CLAUDE.md` for the versioning discipline.

## Hardware

- **Seeed Studio XIAO ESP32-S3**, mounted on the
  [LED Driver Board](https://wiki.seeedstudio.com/led_driver_board/) for
  power and signal conditioning.
- The lambda sensor's raw signal is externally amplified (op-amp) into the
  ESP32's 0–3.3V ADC range, wired to any **ADC1-capable pin** (`GPIO1`–`GPIO10`,
  i.e. `A0`–`A5`/`D0`–`D5`/`D8`–`D10` on the XIAO) — `ADC2` shares hardware
  with the WiFi radio and is unreliable while the access point is running.
- Default calibration assumes a Bosch step lambda sensor behind a 3.2×
  op-amp gain stage (~320–2880 mV usable range); adjust via the Settings
  tab or the auto-calibration wizard if your sensor/gain differs.
- Runs at ignition power (no graceful shutdown) — designed to tolerate
  being cut off at any time; long-term stats are checkpointed to flash
  periodically, not just on shutdown.
- ADC channel, WiFi AP credentials, and calibration defaults are centralized
  in `include/config.h`.

## Usage

1. Power the device — it starts its own WiFi access point (default SSID
   `lambda-monitor`, password `lambda1234`, configurable in
   `include/config.h`).
2. Connect your phone or laptop to that network.
3. Open `http://192.168.4.1` in a browser.
4. **Live** tab: gauge, voltage, switching frequency, control-quality
   average, and session/long-term statistics tables.
5. **Chart** tab: recent voltage history with mixture zones.
6. **Settings** tab: calibration (manual/wizard/auto) and firmware version.
7. **Firmware Update** tab: upload a new `firmware.bin` for OTA flashing.

The sensor needs to warm up before readings are trusted — the device stays
in "Warmup" until either a fixed timeout elapses or the sensor starts
switching (whichever comes first), and warmup time is excluded from the
mixture statistics.

## Building / Development

- [PlatformIO](https://platformio.org/) with the ESP-IDF framework.
- `pio run -e esp32s3` — build the firmware.
- `pio test -e test_native` — run the native unit test suite (no hardware
  needed).
- `python ota_upload.py` — build (if needed) and flash over the air to
  `192.168.4.1` (or a custom IP as the first argument).
- Releases are tagged (`vX.Y.Z`, see `CLAUDE.md`) and built automatically
  by GitHub Actions on tag push, publishing `firmware.bin` to a GitHub
  Release (`.github/workflows/release.yml`).
