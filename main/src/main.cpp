#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"

// From inc
#include "wifi.h"
#include "box_login.h"
#include "download_master_json.h"
#include "sdcard.h"
#include "direction.h"
#include "download_mp3.h"


static const char *TAG = "MAIN";

// Forward declaration for download task
void download_task(void *pvParameters);

extern "C" void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();

    // Wait for Wi-Fi connection before starting the HTTP task
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    // Initialize SD card
    ret = init_sd_card();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SD card");
        return;
    }
    // Get the list of files in a directory on the SD card
    std::vector<std::string> files = list_files("/sdcard/media/audio/");

    // Process the file names as needed
    for (const auto &file_name : files)
    {
            ESP_LOGI("FILE", "Stored File Name: %s", file_name.c_str());
    }

    // Create a task for the initial login HTTP POST request
    xTaskCreate(&http_post_task, "http_post_task", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Main application started");
}

void download_task(void *pvParameters)
{
    char *accessToken = (char *)pvParameters;
    download_master_json(accessToken);
    process_direction_files();
    process_audio_files();
    vTaskDelete(NULL);
}