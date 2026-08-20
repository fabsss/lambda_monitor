#include "web_server.h"
#include "frontend_assets.h"
#include "adc_task.h"
#include "nvs_store.h"
#include "lambda_stats.h"
#include "signal_interpreter.h"
#include "ring_buffer.h"
#include "ota_task.h"

#include "esp_http_server.h"
#include "cJSON.h"
#include <string.h>

static httpd_handle_t s_server = NULL;

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
    lambda_longterm_stats_t stats;
    nvs_store_load_stats(&stats);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "t_warmup_s", stats.t_warmup_s);
    cJSON_AddNumberToObject(root, "t_lean_s", stats.t_lean_s);
    cJSON_AddNumberToObject(root, "t_lambda1_s", stats.t_lambda1_s);
    cJSON_AddNumberToObject(root, "t_rich_s", stats.t_rich_s);
    cJSON_AddNumberToObject(root, "index_min", stats.index_min);
    cJSON_AddNumberToObject(root, "index_max", stats.index_max);
    cJSON_AddNumberToObject(root, "total_runtime_s", stats.total_runtime_s);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t reset_post_handler(httpd_req_t *req)
{
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
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }
    buf[len] = '\0';

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
    nvs_store_save_config(&cal);
    adc_task_set_calibration(&cal);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
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

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        return ESP_OK;
    }

    adc_snapshot_t snap;
    adc_task_get_snapshot(&snap);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "index_fast", snap.index_fast);
    cJSON_AddNumberToObject(root, "index_slow_avg", snap.index_slow_avg);
    cJSON_AddNumberToObject(root, "category", snap.category);
    cJSON_AddNumberToObject(root, "warmup_state", snap.warmup_state);
    cJSON_AddNumberToObject(root, "switches_per_min", snap.switches_per_min);
    cJSON_AddNumberToObject(root, "seconds_since_last_edge", snap.seconds_since_last_edge);

    char *json = cJSON_PrintUnformatted(root);
    httpd_ws_frame_t ws_pkt = { 0 };
    ws_pkt.payload = (uint8_t *)json;
    ws_pkt.len = strlen(json);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    httpd_ws_send_frame(req, &ws_pkt);

    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

void web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    httpd_start(&s_server, &config);

    httpd_uri_t uris[] = {
        { .uri = "/", .method = HTTP_GET, .handler = index_get_handler },
        { .uri = "/style.css", .method = HTTP_GET, .handler = style_get_handler },
        { .uri = "/app.js", .method = HTTP_GET, .handler = app_js_get_handler },
        { .uri = "/api/stats", .method = HTTP_GET, .handler = stats_get_handler },
        { .uri = "/api/reset", .method = HTTP_POST, .handler = reset_post_handler },
        { .uri = "/api/config", .method = HTTP_GET, .handler = config_get_handler },
        { .uri = "/api/config", .method = HTTP_POST, .handler = config_post_handler },
        { .uri = "/uplot.min.js", .method = HTTP_GET, .handler = uplot_js_get_handler },
        { .uri = "/uplot.min.css", .method = HTTP_GET, .handler = uplot_css_get_handler },
        { .uri = "/api/curve", .method = HTTP_GET, .handler = curve_get_handler },
        { .uri = "/api/ota", .method = HTTP_POST, .handler = ota_post_handler },
        { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true },
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_server, &uris[i]);
    }
}
