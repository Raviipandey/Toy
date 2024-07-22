#ifndef DOWNLOAD_MASTER_JSON_H
#define DOWNLOAD_MASTER_JSON_H

#include "esp_err.h"


// Function to initiate the HTTP POST request for master JSON download
void download_master_json(const char *access_token);
void download_task(void *pvParameters);

// Function to get the number of direction files
int get_direction_file_count();

// Function to get the direction file name by index
const char* get_direction_file_name(int index);

// Accessor functions for token and request body
const char* get_access_token();
const char* get_request_body();

//mp3 file in server
int get_N_count();
const char *get_N_value(int index);

#endif // DOWNLOAD_MASTER_JSON_H
