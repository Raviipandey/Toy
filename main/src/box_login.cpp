#include "box_login.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "download_master_json.h"

#define POST_URL "https://uat.littlecubbie.in/auth/v1/box/login"

static const char *TAG = "BOX_LOGIN";
static uint8_t outputBuff[2048] = {0};

uint8_t tokenRequestFlag = 0;
char accessToken[40] = {0};
uint8_t validTokenFlag = 0;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
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
        if (tokenRequestFlag && !validTokenFlag)
        {
            if (memcmp(evt->header_key, "x-cubbies-box-token", 19) == 0)
            {
                if (strlen(evt->header_value) > 40)
                {
                    ESP_LOGI("Error", "Token too long. Max limit = 40 characters");
                    break;
                }
                ESP_LOGI("HeaderFound", "key=%s, value=%s", evt->header_key, evt->header_value);
                strncpy(accessToken, evt->header_value, sizeof(accessToken) - 1);
                accessToken[sizeof(accessToken) - 1] = '\0';
                ESP_LOGI("accessToken", "value=%s", accessToken);
                validTokenFlag = 1;
            }
        }
        break;

    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > sizeof(outputBuff) - 1)
        {
            ESP_LOGE(TAG, "Received data length exceeds buffer size");
            break;
        }
        memcpy(outputBuff, evt->data, evt->data_len);
        outputBuff[evt->data_len] = '\0';
        ESP_LOGI(TAG, "Response Data: %s", outputBuff);
        break;

    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (validTokenFlag)
        {
            xTaskCreate(download_task, "download_task", 8192, (void *)accessToken, 5, NULL);
        }
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

void http_post_task(void *pvParameters)
{
    tokenRequestFlag = 1;
    esp_http_client_config_t config = {
        .url = POST_URL,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    const char *post_data = "{\"macAddress\":\"08:3A:F2:31:2D:8C\",\"boxUniqueId\":\"258ebc0f-2de5-471d-9d83-716cce092625\"}";

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %" PRId64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}