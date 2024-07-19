#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#define DOWNLOAD_URL "https://uat.littlecubbie.in/box/v1/download/masterJson"
#define FILE_PATH "/sdcard/media/toys/RFID_1/metadata/metadata.cubbies"

static const char *TAG = "MASTER_JSON_DOWNLOAD";

static esp_err_t create_directory(const char *path)
{
    char temp_path[256];
    char *p = NULL;
    size_t len;

    snprintf(temp_path, sizeof(temp_path), "%s", path);
    len = strlen(temp_path);
    if (temp_path[len - 1] == '/')
        temp_path[len - 1] = 0;
    for (p = temp_path + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = 0;
            if (mkdir(temp_path, S_IRWXU) != 0 && errno != EEXIST)
            {
                ESP_LOGE(TAG, "Failed to create directory %s: %s", temp_path, strerror(errno));
                return ESP_FAIL;
            }
            *p = '/';
        }
    }
    if (mkdir(temp_path, S_IRWXU) != 0 && errno != EEXIST)
    {
        ESP_LOGE(TAG, "Failed to create directory %s: %s", temp_path, strerror(errno));
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t download_event_handler(esp_http_client_event_t *evt)
{
    static FILE *file = NULL;

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
            if (file == NULL)
            {
                // Create the directory structure if it does not exist
                if (create_directory("/sdcard/media/toys/RFID_1/metadata/") != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to create directories");
                    return ESP_FAIL;
                }

                file = fopen(FILE_PATH, "w");
                if (file == NULL)
                {
                    ESP_LOGE(TAG, "Failed to open file for writing: %s", strerror(errno));
                    return ESP_FAIL;
                }
            }
            fwrite(evt->data, 1, evt->data_len, file);
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (file != NULL)
        {
            fclose(file);
            file = NULL;
            ESP_LOGI(TAG, "Response Data written to file successfully");
        }
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

static esp_err_t init_sd_card(void)
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
    ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK)
    {
        ESP_LOGE("SD_CARD", "Failed to mount filesystem on SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI("SD_CARD", "SD card mounted successfully");
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
