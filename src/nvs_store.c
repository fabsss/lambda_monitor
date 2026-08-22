#include <string.h>
#include "nvs_store.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#define NVS_NAMESPACE "lambda_mon"
#define KEY_STATS "lt_stats"
#define KEY_CONFIG "config"
#define KEY_WIFI "wifi_cred"

static const char *TAG = "nvs_store";

void nvs_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
    }
}

static bool load_blob(const char *key, void *out, size_t expected_size)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    size_t size = expected_size;
    esp_err_t err = nvs_get_blob(handle, key, out, &size);
    nvs_close(handle);

    return err == ESP_OK && size == expected_size;
}

static void save_blob(const char *key, const void *data, size_t size)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(%s) failed: %s", key, esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(handle, key, data, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_blob(%s) failed: %s", key, esp_err_to_name(err));
    } else {
        err = nvs_commit(handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "nvs_commit(%s) failed: %s", key, esp_err_to_name(err));
        }
    }
    nvs_close(handle);
}

void nvs_store_load_stats(lambda_longterm_stats_t *out)
{
    if (load_blob(KEY_STATS, out, sizeof(*out)) && lambda_stats_validate(out)) {
        return;
    }

    /* Current-layout load failed - the blob may have been written by an
     * older firmware whose lambda_longterm_stats_t was a different size
     * (LAMBDA_STATS_VERSION bumped since). Re-read whatever is actually
     * stored and try to recover it via lambda_stats_migrate_legacy()
     * before giving up and resetting to zero. */
    uint8_t raw[sizeof(*out)];
    size_t raw_len = sizeof(raw);
    nvs_handle_t handle;
    bool have_raw = false;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        have_raw = nvs_get_blob(handle, KEY_STATS, raw, &raw_len) == ESP_OK;
        nvs_close(handle);
    }

    if (have_raw && lambda_stats_migrate_legacy(out, raw, raw_len)) {
        ESP_LOGI(TAG, "Migrated long-term stats from an older firmware's on-disk format");
        return;
    }

    lambda_stats_reset(out);
}

void nvs_store_save_stats(const lambda_longterm_stats_t *stats)
{
    lambda_longterm_stats_t to_save = *stats;
    lambda_stats_finalize_crc(&to_save);
    save_blob(KEY_STATS, &to_save, sizeof(to_save));
}

void nvs_store_load_config(si_calibration_t *out)
{
    if (!load_blob(KEY_CONFIG, out, sizeof(*out))) {
        si_default_calibration(out);
    }
}

void nvs_store_save_config(const si_calibration_t *cal)
{
    save_blob(KEY_CONFIG, cal, sizeof(*cal));
}

void nvs_store_reset_longterm(void)
{
    lambda_longterm_stats_t fresh;
    lambda_stats_reset(&fresh);
    lambda_stats_finalize_crc(&fresh);
    save_blob(KEY_STATS, &fresh, sizeof(fresh));
}

void nvs_store_load_wifi_credentials(wifi_ap_credentials_t *out, const char *default_ssid, const char *default_password)
{
    if (load_blob(KEY_WIFI, out, sizeof(*out))) {
        out->ssid[sizeof(out->ssid) - 1] = '\0';
        out->password[sizeof(out->password) - 1] = '\0';

        size_t ssid_len = strlen(out->ssid);
        size_t pass_len = strlen(out->password);
        bool ssid_ok = ssid_len > 0 && ssid_len <= WIFI_AP_SSID_MAX_LEN;
        bool pass_ok = pass_len == 0 || (pass_len >= 8 && pass_len <= WIFI_AP_PASSWORD_MAX_LEN);
        if (ssid_ok && pass_ok) {
            return;
        }
    }

    memset(out, 0, sizeof(*out));
    strncpy(out->ssid, default_ssid, sizeof(out->ssid) - 1);
    strncpy(out->password, default_password, sizeof(out->password) - 1);
}

void nvs_store_save_wifi_credentials(const wifi_ap_credentials_t *creds)
{
    save_blob(KEY_WIFI, creds, sizeof(*creds));
}
