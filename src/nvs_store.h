#ifndef NVS_STORE_H
#define NVS_STORE_H

#include "lambda_stats.h"
#include "signal_interpreter.h"
#include "wifi_ap.h"

void nvs_store_init(void);
void nvs_store_load_stats(lambda_longterm_stats_t *out);
void nvs_store_save_stats(const lambda_longterm_stats_t *stats);
void nvs_store_load_config(si_calibration_t *out);
void nvs_store_save_config(const si_calibration_t *cal);
void nvs_store_reset_longterm(void);

/**
 * @brief Load the SoftAP SSID/password, falling back to defaults.
 *
 * Falls back to default_ssid/default_password (filling *out with them)
 * whenever nothing is stored, the stored blob's size doesn't match (e.g.
 * an older/incompatible firmware wrote it), or the stored ssid/password
 * fail basic structural validity (empty ssid, or a password outside the
 * empty-or-8..64-byte range WPA2 requires) - the last case guards against
 * flash corruption producing a config that would crash-loop the device
 * via esp_wifi_set_config()'s ESP_ERROR_CHECK() on the next boot.
 *
 * @param out              Destination struct.
 * @param default_ssid     Fallback SSID.
 * @param default_password Fallback password ("" for an open network).
 */
void nvs_store_load_wifi_credentials(wifi_ap_credentials_t *out, const char *default_ssid, const char *default_password);
void nvs_store_save_wifi_credentials(const wifi_ap_credentials_t *creds);

#endif
