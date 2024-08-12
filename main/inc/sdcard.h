#include "esp_err.h"
// sdcard.cpp has them
// #include <vector>
// #include <string>

esp_err_t init_sd_card();

void read_file(const char *path);
char** list_files(const char *path, int *file_count);
void free_file_list(char **file_names, int file_count);
