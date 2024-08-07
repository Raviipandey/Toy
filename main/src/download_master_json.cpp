#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include <string.h>
#include <stdlib.h>
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include <sys/stat.h>
#include "cJSON.h"

// From inc
//  #include "download_master_json.h"
#include "main.h"

#define DOWNLOAD_URL "https://uat.littlecubbie.in/box/v1/download/masterJson"
#define FILE_PATH "/sdcard/media/toys/RFID_1/metadata/metadata.cubbies"

static const char *TAG = "MASTER_JSON_DOWNLOAD";

// Dynamic buffer to hold response data
static char *response_buffer = NULL;
static int response_buffer_size = 0;

// Store direction file names
char **direction_file_names = NULL;
int direction_file_count = 0;

// Store N values from media array
char **N_server = NULL;
int N_count = 0;

// Global variables for access token and request body
static const char *request_body = "{\"rfid\":\"E0040350196D3957\",\"userIds\":[\"ec6b8990-8546-4d0d-ad57-871861415639\"],\"macAddress\":\"E0:E2:E6:72:9B:4C\"}";
char baseUrl[256] = {0}; // Initialize the baseUrl variable

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
            response_buffer = (char *)realloc(response_buffer, response_buffer_size + evt->data_len + 1);
            if (response_buffer == NULL)
            {
                ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
                return ESP_FAIL;
            }
            memcpy(response_buffer + response_buffer_size, evt->data, evt->data_len);
            response_buffer_size += evt->data_len;
            response_buffer[response_buffer_size] = '\0';

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
    ESP_LOGI(TAG, "Response Data: %s", response_buffer);

    if (file != NULL)
    {
        fclose(file);
        file = NULL;
        ESP_LOGI(TAG, "Response Data written to metadata.txt file successfully");

        // Call the function to parse JSON response and store metadata
        parse_and_store_metadata(response_buffer, response_buffer_size);
    }

    free(response_buffer);
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
    return ESP_OK; // Ensure the function always returns ESP_OK
}

void download_master_json()
{
    esp_http_client_config_t config = {
        .url = DOWNLOAD_URL,
        .event_handler = download_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "x-cubbies-box-token", accessToken);
    esp_http_client_set_post_field(client, request_body, strlen(request_body));

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

void update_N_server(char **sd_files, int sd_file_count)
{
    char **new_N_server = NULL;
    int new_N_count = 0;

    // Allocate memory for the new N_server array
    new_N_server = (char **)malloc(N_count * sizeof(char *));
    if (new_N_server == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for new N_server");
        return;
    }

    // Compare and copy unique values
    for (int i = 0; i < N_count; i++)
    {
        int found = 0;
        for (int j = 0; j < sd_file_count; j++)
        {
            if (strcmp(N_server[i], sd_files[j]) == 0)
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            new_N_server[new_N_count] = strdup(N_server[i]);
            if (new_N_server[new_N_count] == NULL)
            {
                ESP_LOGE(TAG, "Failed to allocate memory for new N_server value");
                // Free allocated memory before returning
                for (int k = 0; k < new_N_count; k++)
                {
                    free(new_N_server[k]);
                }
                free(new_N_server);
                return;
            }
            new_N_count++;
        }
    }

    // Free the old N_server array
    free_N_server();

    // Update N_server with the new values
    N_server = new_N_server;
    N_count = new_N_count;

    ESP_LOGI(TAG, "Updated N_server array. New count: %d", N_count);
}

void free_N_server()
{
    if (N_server != NULL)
    {
        for (int i = 0; i < N_count; i++)
        {
            free(N_server[i]);
        }
        free(N_server);
        N_server = NULL;
        N_count = 0;
    }
}

const char *get_direction_file_name(int index)
{
    if (index >= 0 && index < direction_file_count)
    {
        return direction_file_names[index];
    }
    return NULL;
}

const char *get_N_value(int index)
{
    if (index >= 0 && index < N_count)
    {
        return N_server[index];
    }
    return NULL;
}