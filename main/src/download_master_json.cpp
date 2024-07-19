#include "download_master_json.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include <string.h>
#include <stdlib.h>

#define DOWNLOAD_URL "https://uat.littlecubbie.in/box/v1/download/masterJson"

static const char *TAG = "MASTER_JSON_DOWNLOAD";

// Dynamic buffer to hold response data
static char *response_buffer = NULL;
static int response_buffer_size = 0;

static esp_err_t download_event_handler(esp_http_client_event_t *evt)
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
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;

    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0)
        {
            response_buffer = (char *)realloc(response_buffer, response_buffer_size + evt->data_len + 1);
            if (response_buffer == NULL)
            {
                ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
                return ESP_FAIL;
            }
            memcpy(response_buffer + response_buffer_size, evt->data, evt->data_len);
            response_buffer_size += evt->data_len;
            response_buffer[response_buffer_size] = '\0';
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        ESP_LOGI(TAG, "Response Data: %s", response_buffer);
        free(response_buffer); // Free the buffer after use
        response_buffer = NULL;
        response_buffer_size = 0;
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        if (response_buffer)
        {
            free(response_buffer);
            response_buffer = NULL;
            response_buffer_size = 0;
        }
        break;
    }
    return ESP_OK;
}

void download_master_json(const char *access_token)
{
    esp_http_client_config_t config = {
        .url = DOWNLOAD_URL,
        .event_handler = download_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    const char *post_data = "{\"rfid\":\"E0040350196D3957\",\"userIds\":[\"ec6b8990-8546-4d0d-ad57-871861415639\"],\"macAddress\":\"E0:E2:E6:72:9B:4C\"}";

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "x-cubbies-box-token", access_token);
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
}
