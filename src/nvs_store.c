#include <string.h>
#include "nvs_store.h"
#include "nvs_flash.h"
#include "nvs.h"

#define NVS_NAMESPACE "lambda_mon"
#define KEY_STATS "lt_stats"
#define KEY_CONFIG "config"

void nvs_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
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
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    nvs_set_blob(handle, key, data, size);
    nvs_commit(handle);
    nvs_close(handle);
}

void nvs_store_load_stats(lambda_longterm_stats_t *out)
{
    if (!load_blob(KEY_STATS, out, sizeof(*out)) || !lambda_stats_validate(out)) {
        lambda_stats_reset(out);
    }
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
