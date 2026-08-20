#include <stdio.h>
#include "adc_task.h"
#include "nvs_store.h"

void app_main(void)
{
    printf("lambda_monitor boot\n");
    nvs_store_init();
    adc_task_start(0);
}
