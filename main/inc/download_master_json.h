#ifndef DOWNLOAD_MASTER_JSON_H
#define DOWNLOAD_MASTER_JSON_H

#include "esp_err.h"

// Function to initiate the HTTP POST request for master JSON download
void download_master_json(const char *access_token);
void download_task(void *pvParameters);
#endif // DOWNLOAD_MASTER_JSON_H
