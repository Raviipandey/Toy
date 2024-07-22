#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "download_master_json.h"

static const char *TAG = "PROCESS_AUDIO_FILES";

static FILE *file = NULL;

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
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (file && evt->data)
            {
                fwrite(evt->data, 1, evt->data_len, file);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (file)
            {
                fclose(file);
                file = NULL;
                ESP_LOGI(TAG, "File download complete");
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            if (file)
            {
                fclose(file);
                file = NULL;
                ESP_LOGE(TAG, "File download interrupted");
            }
            break;
    }
    return ESP_OK;
}

static void download_file(const char *file_name)
{
    if (file_name == NULL)
    {
        ESP_LOGE(TAG, "File name is NULL");
        return;
    }

    char url[256];
    snprintf(url, sizeof(url), "https://uat.littlecubbie.in/box/v1/download/file-download?fileName=%s.mp3", file_name);

    char file_path[256];
    snprintf(file_path, sizeof(file_path), "/sdcard/media/audio/%s.mp3", file_name);

    file = fopen(file_path, "w");
    if (file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", file_path);
        return;
    }

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "x-cubbies-box-token", get_access_token());

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "GET request to %s succeeded. Status = %d, content_length = %" PRId64,
                 url,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "GET request failed: %s", esp_err_to_name(err));
        if (file)
        {
            fclose(file);
            file = NULL;
        }
    }

    esp_http_client_cleanup(client);
}

void process_audio_files()
{
    int file_count = get_N_count();
    ESP_LOGI(TAG, "Processing %d audio files", file_count);

    for (int i = 0; i < file_count; i++)
    {
        const char *file_name = get_N_value(i);
        if (file_name != NULL)
        {
            ESP_LOGI(TAG, "Audio file: %s", file_name);

            // Send GET request to download the file and save the response data
            download_file(file_name);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to get audio file name for index %d", i);
        }
    }

    // Print the access token and request body
    ESP_LOGI(TAG, "Access Token: %s", get_access_token());
    ESP_LOGI(TAG, "Request Body: %s", get_request_body());
}
