#include "web_server.h"
#include "config.h"
#include "frontend_assets.h"
#include "adc_task.h"
#include "nvs_store.h"
#include "lambda_stats.h"
#include "signal_interpreter.h"
#include "ring_buffer.h"
#include "ota_task.h"
#include "wifi_ap.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdbool.h>

static const char *TAG = "web_server";
static httpd_handle_t s_server = NULL;

static double avg2s_average(const lambda_longterm_stats_t *stats)
{
    return stats->avg2s_count > 0 ? (double)stats->avg2s_sum / (double)stats->avg2s_count : 0.0;
}

static bool calibration_is_valid(const si_calibration_t *cal)
{
    if (cal->u_min_mv < ADC_MV_MIN || cal->u_min_mv > ADC_MV_MAX) return false;
    if (cal->u_max_mv < ADC_MV_MIN || cal->u_max_mv > ADC_MV_MAX) return false;
    if (cal->u_lambda1_mv < ADC_MV_MIN || cal->u_lambda1_mv > ADC_MV_MAX) return false;
    if (cal->deadband_mv < 0 || cal->deadband_mv > ADC_MV_MAX) return false;
    if (cal->u_min_mv >= cal->u_max_mv) return false;
    if (cal->u_lambda1_mv <= cal->u_min_mv || cal->u_lambda1_mv >= cal->u_max_mv) return false;
    return true;
}

/* Reads req->content_len bytes into buf (size buf_size, leaving room for a
 * NUL terminator), looping over httpd_req_recv() since a body can arrive
 * across multiple TCP segments. Returns the byte count on success, or -1
 * on error/oversized body (caller should send its own error response). */
static int recv_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    if (req->content_len <= 0 || (size_t)req->content_len >= buf_size) {
        return -1;
    }

    size_t total = 0;
    while (total < (size_t)req->content_len) {
        int received = httpd_req_recv(req, buf + total, req->content_len - total);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (received <= 0) {
            return -1;
        }
        total += received;
    }
    buf[total] = '\0';
    return (int)total;
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t style_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    return httpd_resp_send(req, STYLE_CSS, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t app_js_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    return httpd_resp_send(req, APP_JS, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t stats_get_handler(httpd_req_t *req)
{
    lambda_longterm_stats_t longterm, session;
    nvs_store_load_stats(&longterm);
    adc_task_get_session_stats(&session);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "t_warmup_s", longterm.t_warmup_s);
    cJSON_AddNumberToObject(root, "t_very_lean_s", longterm.t_very_lean_s);
    cJSON_AddNumberToObject(root, "t_lean_s", longterm.t_lean_s);
    cJSON_AddNumberToObject(root, "t_lambda1_s", longterm.t_lambda1_s);
    cJSON_AddNumberToObject(root, "t_rich_s", longterm.t_rich_s);
    cJSON_AddNumberToObject(root, "t_very_rich_s", longterm.t_very_rich_s);
    cJSON_AddNumberToObject(root, "index_min", longterm.index_min);
    cJSON_AddNumberToObject(root, "index_max", longterm.index_max);
    cJSON_AddNumberToObject(root, "avg2s_avg", avg2s_average(&longterm));
    cJSON_AddNumberToObject(root, "total_runtime_s", longterm.total_runtime_s);

    cJSON *session_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(session_obj, "t_warmup_s", session.t_warmup_s);
    cJSON_AddNumberToObject(session_obj, "t_very_lean_s", session.t_very_lean_s);
    cJSON_AddNumberToObject(session_obj, "t_lean_s", session.t_lean_s);
    cJSON_AddNumberToObject(session_obj, "t_lambda1_s", session.t_lambda1_s);
    cJSON_AddNumberToObject(session_obj, "t_rich_s", session.t_rich_s);
    cJSON_AddNumberToObject(session_obj, "t_very_rich_s", session.t_very_rich_s);
    cJSON_AddNumberToObject(session_obj, "index_min", session.index_min);
    cJSON_AddNumberToObject(session_obj, "index_max", session.index_max);
    cJSON_AddNumberToObject(session_obj, "avg2s_avg", avg2s_average(&session));
    cJSON_AddNumberToObject(session_obj, "total_runtime_s", session.total_runtime_s);
    cJSON_AddItemToObject(root, "session", session_obj);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t reset_post_handler(httpd_req_t *req)
{
    adc_task_reset_longterm_stats();
    nvs_store_reset_longterm();
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t config_get_handler(httpd_req_t *req)
{
    si_calibration_t cal;
    nvs_store_load_config(&cal);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "u_min_mv", cal.u_min_mv);
    cJSON_AddNumberToObject(root, "u_max_mv", cal.u_max_mv);
    cJSON_AddNumberToObject(root, "u_lambda1_mv", cal.u_lambda1_mv);
    cJSON_AddNumberToObject(root, "deadband_mv", cal.deadband_mv);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t config_post_handler(httpd_req_t *req)
{
    char buf[256];
    if (recv_body(req, buf, sizeof(buf)) < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON parse error");
        return ESP_FAIL;
    }

    si_calibration_t cal;
    si_default_calibration(&cal);

    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "u_min_mv"))) cal.u_min_mv = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "u_max_mv"))) cal.u_max_mv = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "u_lambda1_mv"))) cal.u_lambda1_mv = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "deadband_mv"))) cal.deadband_mv = item->valueint;

    cJSON_Delete(root);

    if (!calibration_is_valid(&cal)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Calibration values out of range");
        return ESP_FAIL;
    }

    nvs_store_save_config(&cal);
    adc_task_set_calibration(&cal);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t version_get_handler(httpd_req_t *req)
{
    const esp_app_desc_t *desc = esp_app_get_description();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "fw_version", desc->version);
    cJSON_AddStringToObject(root, "build_date", desc->date);
    cJSON_AddStringToObject(root, "build_time", desc->time);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t uplot_js_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    return httpd_resp_send(req, UPLOT_JS, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t uplot_css_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    return httpd_resp_send(req, UPLOT_CSS, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t curve_get_handler(httpd_req_t *req)
{
    static int32_t values[RING_BUFFER_CAPACITY];
    static uint32_t timestamps[RING_BUFFER_CAPACITY];
    uint16_t count;
    adc_task_get_curve(values, timestamps, RING_BUFFER_CAPACITY, &count);

    cJSON *root = cJSON_CreateObject();
    cJSON *values_arr = cJSON_CreateArray();
    cJSON *ts_arr = cJSON_CreateArray();
    for (uint16_t i = 0; i < count; i++) {
        cJSON_AddItemToArray(values_arr, cJSON_CreateNumber(values[i]));
        cJSON_AddItemToArray(ts_arr, cJSON_CreateNumber(timestamps[i]));
    }
    cJSON_AddItemToObject(root, "index_values", values_arr);
    cJSON_AddItemToObject(root, "timestamps_s", ts_arr);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    return ota_task_handle_upload(req);
}

static esp_err_t snapshot_get_handler(httpd_req_t *req)
{
    adc_snapshot_t snap;
    adc_task_get_snapshot(&snap);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "index_fast", snap.index_fast);
    cJSON_AddNumberToObject(root, "index_slow_avg", snap.index_slow_avg);
    cJSON_AddNumberToObject(root, "index_avg_2s", snap.index_avg_2s);
    cJSON_AddNumberToObject(root, "category", snap.category);
    cJSON_AddNumberToObject(root, "warmup_state", snap.warmup_state);
    cJSON_AddNumberToObject(root, "switches_per_min", snap.switches_per_min);
    cJSON_AddNumberToObject(root, "seconds_since_last_edge", snap.seconds_since_last_edge);
    cJSON_AddNumberToObject(root, "raw_mv", snap.raw_mv);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t autocal_post_handler(httpd_req_t *req)
{
    char buf[128];
    if (recv_body(req, buf, sizeof(buf)) < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON parse error");
        return ESP_FAIL;
    }

    cJSON *action_item = cJSON_GetObjectItem(root, "action");
    if (!action_item || !action_item->valuestring) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing action");
        return ESP_FAIL;
    }

    const char *action = action_item->valuestring;
    cJSON_Delete(root);

    if (strcmp(action, "start") == 0) {
        adc_task_autocal_start();
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    } else if (strcmp(action, "derive") == 0) {
        si_calibration_t cal;
        adc_task_autocal_stop_and_derive(&cal);
        nvs_store_save_config(&cal);
        adc_task_set_calibration(&cal);

        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "u_min_mv", cal.u_min_mv);
        cJSON_AddNumberToObject(resp, "u_max_mv", cal.u_max_mv);
        cJSON_AddNumberToObject(resp, "u_lambda1_mv", cal.u_lambda1_mv);
        cJSON_AddNumberToObject(resp, "deadband_mv", cal.deadband_mv);
        char *json = cJSON_PrintUnformatted(resp);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
        cJSON_free(json);
        cJSON_Delete(resp);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown action");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t autocal_get_handler(httpd_req_t *req)
{
    uint32_t count = adc_task_autocal_count();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "count", count);
    cJSON_AddNumberToObject(root, "max", adc_task_autocal_max());

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Captive portal: redirect unknown URIs to home page.
 * Catches requests to captive.apple.com, connectivitycheck.gstatic.com/
 * generate_204, msftconnecttest.com/connecttest.txt, etc. - deliberately
 * NOT a 204/200 "success" response. This AP has no internet uplink, and
 * answering the OS's connectivity check with "success" makes the phone
 * treat this Wi-Fi as a full internet connection, which can lead it to
 * disable/deprioritize mobile data. Redirecting instead makes the OS
 * correctly detect "no internet, captive portal" - it keeps mobile data
 * active for real internet access and (on most OSes) offers a "sign in
 * to network" prompt that conveniently opens straight to this device's
 * web UI. Do not special-case /generate_204 (or similar) back to a bare
 * 204/200 - that reintroduces the mobile-data-drops-out bug. */
static esp_err_t captive_portal_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* Reports the SoftAP credentials currently in effect (custom if saved,
 * otherwise the compiled-in default) - mirrors the fallback wifi_ap_start()
 * applies at boot, so this always reflects reality. Password is never
 * echoed back; only whether one is set. */
static esp_err_t wifi_get_handler(httpd_req_t *req)
{
    wifi_ap_credentials_t creds;
    nvs_store_load_wifi_credentials(&creds, WIFI_AP_SSID, WIFI_AP_PASSWORD);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ssid", creds.ssid);
    cJSON_AddBoolToObject(root, "has_password", strlen(creds.password) > 0);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Same constraints esp_wifi_set_config() enforces for WIFI_MODE_AP - reject
 * anything outside them here so an invalid config can never be persisted
 * (nvs_store_load_wifi_credentials() re-checks this on load as a second
 * line of defense, but the point is to never need that fallback). */
static bool wifi_credentials_is_valid(const char *ssid, const char *password)
{
    size_t ssid_len = strlen(ssid);
    size_t pass_len = strlen(password);
    if (ssid_len == 0 || ssid_len > WIFI_AP_SSID_MAX_LEN) return false;
    if (pass_len != 0 && (pass_len < 8 || pass_len > WIFI_AP_PASSWORD_MAX_LEN)) return false;
    return true;
}

/* Saves new SoftAP credentials and reboots to apply them (like OTA, the AP
 * has to restart to broadcast under the new SSID/password, so clients
 * reconnect manually afterward). Password is required on every save - an
 * empty password means "make this an open network", not "keep the
 * current one" (same convention wifi_ap_start() already used for the
 * compiled-in default). */
static esp_err_t wifi_post_handler(httpd_req_t *req)
{
    /* Room for a 32-byte SSID and a 64-byte password plus JSON framing,
     * with headroom for characters that need JSON-escaping (e.g. a
     * password containing a literal quote or backslash). */
    char buf[256];
    if (recv_body(req, buf, sizeof(buf)) < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON parse error");
        return ESP_FAIL;
    }

    cJSON *ssid_item = cJSON_GetObjectItem(root, "ssid");
    cJSON *password_item = cJSON_GetObjectItem(root, "password");
    const char *ssid = (ssid_item && cJSON_IsString(ssid_item)) ? ssid_item->valuestring : "";
    const char *password = (password_item && cJSON_IsString(password_item)) ? password_item->valuestring : "";

    if (!wifi_credentials_is_valid(ssid, password)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            "SSID must be 1-32 chars; password must be empty (open network) or 8-64 chars");
        return ESP_FAIL;
    }

    wifi_ap_credentials_t creds = { 0 };
    strncpy(creds.ssid, ssid, sizeof(creds.ssid) - 1);
    strncpy(creds.password, password, sizeof(creds.password) - 1);
    cJSON_Delete(root);

    nvs_store_save_wifi_credentials(&creds);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true,\"rebooting\":true}", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

void web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return;
    }

    httpd_uri_t uris[] = {
        { .uri = "/", .method = HTTP_GET, .handler = index_get_handler },
        { .uri = "/style.css", .method = HTTP_GET, .handler = style_get_handler },
        { .uri = "/app.js", .method = HTTP_GET, .handler = app_js_get_handler },
        { .uri = "/api/stats", .method = HTTP_GET, .handler = stats_get_handler },
        { .uri = "/api/reset", .method = HTTP_POST, .handler = reset_post_handler },
        { .uri = "/api/config", .method = HTTP_GET, .handler = config_get_handler },
        { .uri = "/api/config", .method = HTTP_POST, .handler = config_post_handler },
        { .uri = "/api/snapshot", .method = HTTP_GET, .handler = snapshot_get_handler },
        { .uri = "/api/calibrate/auto", .method = HTTP_GET, .handler = autocal_get_handler },
        { .uri = "/api/calibrate/auto", .method = HTTP_POST, .handler = autocal_post_handler },
        { .uri = "/uplot.min.js", .method = HTTP_GET, .handler = uplot_js_get_handler },
        { .uri = "/uplot.min.css", .method = HTTP_GET, .handler = uplot_css_get_handler },
        { .uri = "/api/curve", .method = HTTP_GET, .handler = curve_get_handler },
        { .uri = "/api/ota", .method = HTTP_POST, .handler = ota_post_handler },
        { .uri = "/api/version", .method = HTTP_GET, .handler = version_get_handler },
        { .uri = "/api/wifi", .method = HTTP_GET, .handler = wifi_get_handler },
        { .uri = "/api/wifi", .method = HTTP_POST, .handler = wifi_post_handler },
        { .uri = "/*", .method = HTTP_GET, .handler = captive_portal_handler },
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_server, &uris[i]);
    }
}
