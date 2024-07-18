#include "http_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include <string.h>

#define POST_URL "https://uat.littlecubbie.in/auth/v1/box/login"

static const char *TAG = "HTTP_CLIENT";
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
            // ESP_LOGI("LOG", "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
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
            // ESP_LOGW(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
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

    vTaskDelay(5000 / portTICK_PERIOD_MS);

    esp_http_client_set_url(client, POST_URL);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
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

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}