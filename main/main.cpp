#include <stdio.h>
#include <string.h>
#include "esp_mac.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"

// Define the URL for the HTTP POST request
#define POST_URL "https://uat.littlecubbie.in/auth/v1/box/login"

// Tag for logging
static const char *TAG = "HTTP_CLIENT";
// static uint8_t outputBuff[DOWNLOAD_CHUNK_SIZE] = {0};
static uint8_t outputBuff[2048] = {0};

uint8_t tokenRequestFlag = 0;
char accessToken[40] = {0};
uint8_t validTokenFlag = 0;

extern "C"
{

    // Event handler for detailed debugging
    esp_err_t http_event_handler(esp_http_client_event_t *evt)
    {
        switch (evt->event_id)
        {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;

        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;

        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;

        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;

        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI("LOG", "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);

            if (tokenRequestFlag && (validTokenFlag == 0))
            {
                if (memcmp(evt->header_key, "x-cubbies-box-token", 19) == 0) // First 19 bytes are the same
                {
                    if (strlen(evt->header_value) > 50)
                    {
                        ESP_LOGI("Error", "tokenLen=%d, too long. Max limit = 1kb", strlen(evt->header_value));
                        break;
                    }
                    ESP_LOGI("\n HeaderFound", "\n key=%s, \n value=%s, \n keyLen=%d, \n tokenLen=%d", evt->header_key, evt->header_value, strlen(evt->header_key), strlen(evt->header_value));
                    memcpy(accessToken, evt->header_value, strlen(evt->header_value));

                    ESP_LOGI("\n accessToken", "value=%s", evt->header_value);
                    validTokenFlag = 1;
                }
            }
            break;

        case HTTP_EVENT_ON_DATA:
            ESP_LOGW(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // ESP_LOGW("L", "l=%d", evt->data_len);
            memcpy(outputBuff, evt->data, evt->data_len);
            // ESP_LOGW(TAG, "%s", outputBuff);
            outputBuff[evt->data_len] = '\0';
            ESP_LOGI(TAG, "Response data NEW: %s", outputBuff);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        }
        return ESP_OK;
    }

    // HTTP POST task
    void http_post_task(void *pvParameters)
    {
        tokenRequestFlag = 1;
        // Configure the HTTP client
        esp_http_client_config_t config = {
            .url = POST_URL,
            .event_handler = http_event_handler, // Add event handler for detailed debugging
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            //.timeout_ms = 5000, // Set a timeout for the request
            .crt_bundle_attach = esp_crt_bundle_attach,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        // JSON data to post
        const char *post_data = "{\"macAddress\":\"08:3A:F2:31:2D:8C\",\"boxUniqueId\":\"258ebc0f-2de5-471d-9d83-716cce092625\"}";

        // Add a delay before making the HTTP request
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        // Set HTTP method and headers
        esp_http_client_set_url(client, POST_URL);
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, post_data, strlen(post_data));

        // Perform HTTP POST request
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK)
        {
            // Log status and response
            ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %" PRId64,
                     esp_http_client_get_status_code(client),
                     esp_http_client_get_content_length(client));
            char buffer[1024] = {0};
            int data_read = esp_http_client_read_response(client, buffer, sizeof(buffer));
            if (data_read >= 0)
            {
                ESP_LOGI(TAG, "HTTP POST Response: %s", buffer);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
        else
        {
            ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        }

        // Cleanup
        esp_http_client_cleanup(client);
        vTaskDelete(NULL);
    }

    // Event handler for Wi-Fi events
    void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        if (event_base == WIFI_EVENT)
        {
            switch (event_id)
            {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "Disconnected. Trying to reconnect...");
                esp_wifi_connect();
                break;
            default:
                break;
            }
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Wi-Fi connected successfully");
            xTaskCreate(&http_post_task, "http_post_task", 8192, NULL, 5, NULL);
        }
    }

    // Initialize Wi-Fi station mode
    void wifi_init_sta(void)
    {
        // Initialize ESP network interface and event loop
        esp_netif_init();
        esp_event_loop_create_default();
        esp_netif_create_default_wifi_sta();

        // Register event handlers for Wi-Fi and IP events
        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_got_ip));

        // Initialize Wi-Fi with default config
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);

        // Wi-Fi configuration (replace with your SSID and password)
        wifi_config_t wifi_config = {
            .sta = {
                .ssid = "Aloha",
                .password = "Aloha2022",
            },
        };
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "Wi-Fi initialized, connecting to network...");
        //   vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

} // extern "C"

// Main application function
extern "C" void app_main()
{
    // Initialize NVS flash memory
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Start Wi-Fi initialization
    wifi_init_sta();
}