#include <stdio.h>
#include "adc_task.h"
#include "nvs_store.h"
#include "wifi_ap.h"
#include "web_server.h"
#include "ota_task.h"

void app_main(void)
{
    printf("lambda_monitor boot\n");
    nvs_store_init();
    adc_task_start(0);
    wifi_ap_start("lambda-monitor", "lambda1234");
    web_server_start();
    ota_task_mark_valid_if_pending();
}
