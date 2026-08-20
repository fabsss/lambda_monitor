#ifndef OTA_TASK_H
#define OTA_TASK_H

#include "esp_http_server.h"

void ota_task_mark_valid_if_pending(void);
esp_err_t ota_task_handle_upload(httpd_req_t *req);

#endif
