#include <stdio.h>
#include "adc_task.h"
#include "nvs_store.h"
#include "wifi_ap.h"
#include "web_server.h"
#include "ota_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void rollback_health_check_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(30000));
    ota_task_mark_valid_if_pending();
    vTaskDelete(NULL);
}

void app_main(void)
{
    printf("lambda_monitor boot\n");
    nvs_store_init();
    adc_task_start(0);
    wifi_ap_start("lambda-monitor", "lambda1234");
    web_server_start();
    xTaskCreate(rollback_health_check_task, "health_check", 2048, NULL, 2, NULL);
}
