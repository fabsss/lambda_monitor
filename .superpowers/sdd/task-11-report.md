# Task 11 Report: WiFi SoftAP + HTTP/WebSocket Server with REST Endpoints

**Status:** DONE
**Commit:** dfbf367678ab43d38a5717d22026f7025c5440ef

## Summary

Implemented the four spec files exactly as given:

- `src/frontend_assets.h` — stub `INDEX_HTML` placeholder (Task 12 will replace it)
- `src/wifi_ap.h` / `src/wifi_ap.c` — `wifi_ap_start(ssid, password)` bringing up SoftAP via
  `esp_wifi`/`esp_netif`/`esp_event`
- `src/web_server.h` / `src/web_server.c` — `web_server_start(void)` registering 6 httpd
  handlers: `GET /`, `GET /api/stats`, `POST /api/reset`, `GET /api/config`,
  `POST /api/config`, `GET /ws` (websocket)
- `src/main.c` — updated `app_main` to call `wifi_ap_start("lambda-monitor", "lambda1234")`
  and `web_server_start()` after `nvs_store_init()` + `adc_task_start(0)`

## Deviation from the 4-file/5-file plan

Two additional files were touched, both required to make the build succeed:

- `sdkconfig.defaults` (new) — sets `CONFIG_HTTPD_WS_SUPPORT=y`
- `sdkconfig.esp32s3` (modified) — flipped `# CONFIG_HTTPD_WS_SUPPORT is not set` to
  `CONFIG_HTTPD_WS_SUPPORT=y`

Reason: the initial build failed because `httpd_ws_frame_t`, `HTTPD_WS_TYPE_TEXT`, and
`httpd_ws_send_frame` are gated behind `CONFIG_HTTPD_WS_SUPPORT`, which was off in the
already-generated `sdkconfig.esp32s3`. This is exactly the contingency called out in the
project's implementation plan (`docs/superpowers/plans/2026-08-20-lambda-monitor.md`,
Task 11 Step 7): "if ... `esp_http_server` component config is missing, add
`CONFIG_HTTPD_WS_SUPPORT=y` to a new `sdkconfig.defaults` file at the project root and
re-run." Since `sdkconfig.esp32s3` already existed (defaults only seed a config on first
generation), the existing file also had to be edited directly for the change to take effect
without a clean rebuild.

## Build verification

`pio run -e esp32s3` → **SUCCESS** (254s). Flash usage 79.9% (837512/1048576 bytes), RAM
12.1% (39504/327680 bytes).

`pio test -e test_native` → environment error (`'gcc' is not recognized...`), unrelated to
this change — no `gcc` on PATH in this shell session. Per the task instructions this task has
no host tests anyway (hardware-dependent code).

## Files changed (8, one commit)

- `sdkconfig.defaults` (new)
- `sdkconfig.esp32s3` (modified — 1 line)
- `src/frontend_assets.h` (new)
- `src/main.c` (modified)
- `src/web_server.c` (new)
- `src/web_server.h` (new)
- `src/wifi_ap.c` (new)
- `src/wifi_ap.h` (new)
