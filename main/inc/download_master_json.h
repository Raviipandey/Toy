#ifndef DOWNLOAD_MASTER_JSON_H
#define DOWNLOAD_MASTER_JSON_H

#include "esp_err.h"

// Declare the variables as extern to make them accessible across the project
// extern char **direction_file_names;
extern int direction_file_count;

extern char **N_server;
extern int N_count;

extern char baseUrl[256];
    
// Function to initiate the HTTP POST request for master JSON download
void download_master_json();
void download_task(void *pvParameters);

// Function to get the direction file name by index
const char *get_direction_file_name(int index);////CHECK

// mp3 file in server
const char *get_N_value(int index);////CHECK

void update_N_server(char **sd_files, int sd_file_count);
void free_N_server();

#endif // DOWNLOAD_MASTER_JSON_H
