#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include <stdlib.h>
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"

// From inc
// #include "direction.h"
//  #include "download_master_json.h"
#include "main.h"

static const char *TAG = "PROCESS_DIRECTION_FILES";

static char output_buffer[4096];  // Increased buffer size
static int output_len = 0;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
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
            if (output_len + evt->data_len < sizeof(output_buffer)) {
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
                output_len += evt->data_len;
                output_buffer[output_len] = 0;  
            } else {
                ESP_LOGE(TAG, "Buffer overflow");
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_len > 0) {
                ESP_LOGI(TAG, "Response Data: %s", output_buffer);
            }
            output_len = 0;  // Reset buffer
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            output_len = 0;  // Reset buffer
            break;
    }
    return ESP_OK;
}

static void save_file_to_sd(const char *file_name, const char *data)
{
    if (file_name == NULL || data == NULL)
    {
        ESP_LOGE(TAG, "File name or data is NULL");
        return;
    }

    char file_path[256];
    snprintf(file_path, sizeof(file_path), "/sdcard/media/toys/RFID_1/metadata/%s.txt", file_name);

    FILE *file = fopen(file_path, "w");
    if (file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", file_path);
        return;
    }

    fprintf(file, "%s", data);
    fclose(file);

    ESP_LOGI(TAG, "File saved: %s", file_path);
}

static void download_file(const char *file_name)
{
    if (file_name == NULL)
    {
        ESP_LOGE(TAG, "File name is NULL");
        return;
    }

    // Reset buffer before each request
    memset(output_buffer, 0, sizeof(output_buffer));
    output_len = 0;

    char url[256];
    snprintf(url, sizeof(url), "https://uat.littlecubbie.in/box/v1/download/file-download?fileName=%s", file_name);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "x-cubbies-box-token", accessToken);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "GET request to %s succeeded. Status = %d, content_length = %" PRId64,
                 url,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));

        // Extract the direction from the file name
        char direction[256];
        snprintf(direction, sizeof(direction), "%s", file_name);

        // Find the last occurrence of '_' and extract the part after it
        char *last_underscore = strrchr(direction, '_');
        if (last_underscore != NULL)
        {
            // Extract the direction part
            char *direction_part = last_underscore + 1;
            char *extension_dot = strchr(direction_part, '.');
            if (extension_dot != NULL)
            {
                *extension_dot = '\0';  // Remove the file extension
            }

            // Save the response data to the SD card with direction as the file name
            save_file_to_sd(direction_part, output_buffer);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to parse direction from file name: %s", file_name);
        }
    }
    else
    {
        ESP_LOGE(TAG, "GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void process_direction_files()
{
    int file_count = get_direction_file_count();
    ESP_LOGI(TAG, "Processing %d direction files", file_count);

    for (int i = 0; i < file_count; i++)
    {
        const char* file_name = get_direction_file_name(i);
        if (file_name != NULL)
        {
            ESP_LOGI(TAG, "Direction file: %s", file_name);

            // Send GET request to download the file and save the response data
            download_file(file_name);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to get direction file name for index %d", i);
        }
    }
}