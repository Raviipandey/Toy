#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "wifi.h"
#include "http_client.h"

static const char *TAG = "MAIN";

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

    xTaskCreate(&http_post_task, "http_post_task", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Main application started");
}