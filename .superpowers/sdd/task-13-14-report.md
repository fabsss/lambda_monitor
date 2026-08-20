# Tasks 13-14 Report: Oscilloscope Chart (uPlot) + OTA Firmware Update

## Status: DONE

## Task 13: Oscilloscope Chart (uPlot)

### Files created
- `web/uplot.min.js` — vendored uPlot v1.6.24 IIFE build (downloaded via jsDelivr CDN mirror
  of the npm package, matching the GitHub release asset).
- `web/uplot.min.css` — vendored uPlot v1.6.24 stylesheet.

### Files modified
- `web/index.html` — added `<link rel="stylesheet" href="/uplot.min.css">`, replaced the
  `#tab-chart` section with time-window buttons (10s/30s/60s) + chart container + freeze
  button, added `<script src="/uplot.min.js">` before `app.js`.
- `tools/embed_frontend.py` — added `UPLOT_JS`/`UPLOT_CSS` to the `FILES` list. Also fixed
  `c_escape()` to escape every `?` character as `\?`. Without this, the vendored minified JS
  contains `??=` (nullish-coalescing assignment) which, once embedded verbatim in a C string
  literal, forms the `??=` trigraph sequence; `-Wall -Wextra` (via `-Werror=trigraphs`) failed
  the build. Escaping `?` universally is safe (valid C escape, same character) and fixes this
  for any future vendored asset containing `?`-sequences.
- `src/adc_task.h` / `src/adc_task.c` — added `adc_task_get_curve()`, reading the ring buffer
  under `s_mutex` and copying up to `max_points` chronological samples out.
- `src/web_server.c` — added `uplot_js_get_handler`, `uplot_css_get_handler`,
  `curve_get_handler` (serves `/api/curve` as JSON `{index_values, timestamps_s}` from the
  ring buffer), registered the three new routes, added `#include "ring_buffer.h"`.
- `web/app.js` — added uPlot chart initialization, 1s polling of `/api/curve` filtered by the
  selected time window, time-window button handlers, and freeze/resume toggle.

### Verification
- `pio run -e esp32s3` — SUCCESS (RAM 13.5%, Flash 85.2% of the then-current single-slot
  partition table).

### Commit
`cddb99b` — `feat: add oscilloscope chart screen with uPlot, time-window selection, and freeze`

---

## Task 14: OTA Firmware Update with Dual-Partition Rollback

### Files created
- `partitions.csv` — dual OTA-slot partition table (`nvs`, `otadata`, `ota_0`, `ota_1`).
  **Deviated from the spec's literal offsets**: the spec placed `ota_0` at `0x10000`, but
  `otadata` (offset `0xf000`, size `0x2000`) ends at `0x11000`, so the two partitions
  overlapped and the ESP-IDF partition-table generator rejected the build (`gen_esp32part.py`:
  "Partitions overlap. Partition sets offset 0x10000. Previous partition ends 0x11000").
  Fixed by moving `ota_0` to `0x20000` and `ota_1` to `0x320000` (each still `0x300000`
  bytes), which is consistent with ESP-IDF's own two-OTA-slot reference layout.
- `src/ota_task.h` / `src/ota_task.c` — `ota_task_mark_valid_if_pending()` (cancels the
  ESP-IDF rollback timer once the newly booted image is confirmed to be running normally) and
  `ota_task_handle_upload()` (streams an uploaded `.bin` into the inactive OTA partition via
  `esp_ota_begin`/`esp_ota_write`/`esp_ota_end`, sets it as the boot partition, and reboots).

### Files modified
- `platformio.ini` — added `board_build.partitions = partitions.csv` under `[env:esp32s3]`.
- `src/web_server.c` — added `ota_post_handler` wired to `POST /api/ota`, increased
  `httpd_config_t.max_uri_handlers` to 12 to fit the new routes, added
  `#include "ota_task.h"`.
- `src/main.c` — added `#include "ota_task.h"` and a call to
  `ota_task_mark_valid_if_pending()` after `web_server_start()`.
- `web/index.html` — added the "Firmware Update" nav tab and `#tab-ota` section (file input +
  upload button + progress bar).
- `web/app.js` — added the OTA form submit handler: confirms with the user, then `XHR`-uploads
  the selected file to `/api/ota` with upload progress tracked via the `progress` event.

### Verification
- `pio run -e esp32s3` — SUCCESS (RAM 13.5%, Flash 28.7% of the new, larger dual-OTA-slot
  partition table — 3,145,728 bytes available per app slot's addressable max vs. 1,048,576
  before).
- Note: a pre-existing, unrelated build warning persists — "Flash memory size mismatch
  detected. Expected 8MB, found 2MB!" — this comes from `sdkconfig.esp32s3`/board flash-size
  config vs. actual detected chip size, present before this task's changes, and does not
  block the build.

### Commit
`f7da428` — `feat: add OTA firmware update with dual-partition rollback protection`

---

## Summary

Both tasks implemented, built successfully, and committed sequentially.

**Commit range:** `cddb99b` (Task 13) .. `f7da428` (Task 14)

**Return status: DONE**
