#include "ota_task.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ota_task";

void ota_task_mark_valid_if_pending(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_ota_mark_app_valid_cancel_rollback();
            ESP_LOGI(TAG, "OTA image marked valid");
        }
    }
}

esp_err_t ota_task_handle_upload(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        return ESP_FAIL;
    }

    char buf[1024];
    int remaining = req->content_len;

    while (remaining > 0) {
        int received = httpd_req_recv(req, buf, sizeof(buf));
        if (received <= 0) {
            esp_ota_abort(ota_handle);
            return ESP_FAIL;
        }
        esp_ota_write(ota_handle, buf, received);
        remaining -= received;
    }

    if (esp_ota_end(ota_handle) != ESP_OK) {
        return ESP_FAIL;
    }

    if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
        return ESP_FAIL;
    }

    httpd_resp_send(req, "{\"ok\":true,\"rebooting\":true}", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}
