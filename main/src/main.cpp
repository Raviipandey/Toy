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
#include "sdcard.h"  // Include the header for SD card functions

static const char *TAG = "MAIN";

// Forward declarations
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

    // Initialize Wi-Fi
    wifi_init_sta();

    // Wait for Wi-Fi connection
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    // Initialize SD card
    ret = init_sd_card();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SD card");
        return;
    }

    // Create a task for the initial login HTTP POST request
    xTaskCreate(&http_post_task, "http_post_task", 8192, NULL, 5, NULL);

    // Create a task for downloading data
    // Pass the access token as a parameter
    const char *access_token = "077f03a7-979c-4183-b455-7fef44d1139a";
    xTaskCreate(&download_task, "download_task", 8192, (void *)access_token, 5, NULL);

    ESP_LOGI(TAG, "Main application started");
}

void download_task(void *pvParameters)
{
    char *accessToken = (char *)pvParameters;
    download_master_json(accessToken);
    vTaskDelete(NULL);
}