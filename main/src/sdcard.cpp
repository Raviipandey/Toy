// sdcard.cpp
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "sdcard.h"
#include <dirent.h>
#include <cstdio>
#include <vector>
#include <string>

#define SD_CARD_MOUNT_POINT "/sdcard"

static const char *TAG = "SD_CARD";

esp_err_t init_sd_card()
{
    esp_err_t ret;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    sdmmc_card_t *card;

    // Set the width to 1-line mode
    slot_config.width = 1;

    // Options for mounting the filesystem.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,  // Format the card if mount fails
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Mount the filesystem
    ret = esp_vfs_fat_sdmmc_mount(SD_CARD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount filesystem on SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted successfully");
    return ESP_OK;
}

void read_file(const char *path)
{
    // Open the file for reading
    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", path);
        return;
    }

    ESP_LOGI(TAG, "Reading file: %s", path);
    char line[128];
    while (fgets(line, sizeof(line), f) != NULL)
    {
        // Print each line
        printf("%s", line);
    }

    // Close the file
    fclose(f);
}

std::vector<std::string> list_files(const char *path)
{
    std::vector<std::string> file_names;
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        return file_names;
    }

    struct dirent *entry;
    ESP_LOGI(TAG, "Listing files in directory: %s", path);
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG) // If the entry is a regular file
        {
            file_names.push_back(entry->d_name);
            // ESP_LOGI(TAG, "File: %s", entry->d_name);
        }
        else if (entry->d_type == DT_DIR) // If the entry is a directory
        {
            ESP_LOGI(TAG, "Directory: %s", entry->d_name);
        }
    }

    closedir(dir);
    return file_names;
}