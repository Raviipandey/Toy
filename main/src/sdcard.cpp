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
#include <cstring>
#include <cstdlib>
#include "download_master_json.h"

#define SD_CARD_MOUNT_POINT "/sdcard"

static const char *TAG = "SD_CARD";
#define MAX_FILES 100

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

char** list_files(const char *path, int *file_count)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        *file_count = 0;
        return NULL;
    }

    char **file_names = (char **)malloc(MAX_FILES * sizeof(char *));
    if (file_names == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for file names");
        closedir(dir);
        *file_count = 0;
        return NULL;
    }

    struct dirent *entry;
    int count = 0;
    ESP_LOGI(TAG, "Listing files in directory: %s", path);
    while ((entry = readdir(dir)) != NULL && count < MAX_FILES)
    {
        if (entry->d_type == DT_REG) // If the entry is a regular file
        {
            file_names[count] = strdup(entry->d_name);
            if (file_names[count] == NULL)
            {
                ESP_LOGE(TAG, "Failed to allocate memory for file name");
                break;
            }
            count++;
        }
        else if (entry->d_type == DT_DIR) // If the entry is a directory
        {
            ESP_LOGI(TAG, "Directory: %s", entry->d_name);
        }
    }

    closedir(dir);
    *file_count = count;
    return file_names;
}

// Add this function to free the memory allocated for file names
void free_file_list(char **file_names, int file_count)
{
    if (file_names != NULL)
    {
        for (int i = 0; i < file_count; i++)
        {
            free(file_names[i]);
        }
        free(file_names);
    }
}


