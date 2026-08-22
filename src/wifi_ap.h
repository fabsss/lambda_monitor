#ifndef WIFI_AP_H
#define WIFI_AP_H

/* ESP-IDF/WPA2 hard limits: SSID up to 32 bytes, WPA2-PSK password either
 * empty (open network) or 8-64 bytes - esp_wifi_set_config() rejects
 * anything outside that, so these are validated against on save (see
 * web_server.c) to avoid ever persisting a config that would fail
 * ESP_ERROR_CHECK() on the next boot. */
#define WIFI_AP_SSID_MAX_LEN 32
#define WIFI_AP_PASSWORD_MAX_LEN 64

typedef struct __attribute__((packed)) {
    char ssid[WIFI_AP_SSID_MAX_LEN + 1];
    char password[WIFI_AP_PASSWORD_MAX_LEN + 1];
} wifi_ap_credentials_t;

/**
 * @brief Start the SoftAP.
 *
 * Uses whatever custom SSID/password is saved in NVS (see
 * nvs_store_load_wifi_credentials()); falls back to default_ssid/
 * default_password if none is saved or the saved entry fails validation
 * (corrupt/short-circuits a boot crash from an invalid WPA2 config).
 *
 * @param default_ssid     Fallback SSID.
 * @param default_password Fallback password ("" for an open network).
 */
void wifi_ap_start(const char *default_ssid, const char *default_password);

#endif
