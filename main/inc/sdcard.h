#include "esp_err.h"
#include <vector>
#include <string>

using namespace std;
esp_err_t init_sd_card();
void read_file(const char *path);
vector<string> list_files(const char *path);