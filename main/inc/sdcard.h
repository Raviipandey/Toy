#include "esp_err.h"
#include <vector>
#include <string>

esp_err_t init_sd_card();
void read_file(const char *path);
std::vector<std::string> list_files(const char *path);