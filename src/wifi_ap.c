#include "wifi_ap.h"
#include "nvs_store.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"

void wifi_ap_start(const char *default_ssid, const char *default_password)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_ap_credentials_t creds;
    nvs_store_load_wifi_credentials(&creds, default_ssid, default_password);

    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.ap.ssid, creds.ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(creds.ssid);
    strncpy((char *)wifi_config.ap.password, creds.password, sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = strlen(creds.password) == 0 ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
