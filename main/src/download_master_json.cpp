#include "download_master_json.h"
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

#define DOWNLOAD_URL "https://uat.littlecubbie.in/box/v1/download/masterJson"
#define FILE_PATH "/sdcard/media/toys/RFID_1/metadata/metadata.txt"

static const char *TAG = "MASTER_JSON_DOWNLOAD";

// Dynamic buffer to hold response data
static char *response_buffer = NULL;
static int response_buffer_size = 0;

// Store direction file names
static char **direction_file_names = NULL;
static int direction_file_count = 0;

// Store N values from media array
static char **N_server = NULL;
static int N_count = 0;

// Global variables for access token and request body
static const char *global_access_token = NULL;
static const char *global_request_body = "{\"rfid\":\"E0040350196D3957\",\"userIds\":[\"ec6b8990-8546-4d0d-ad57-871861415639\"],\"macAddress\":\"E0:E2:E6:72:9B:4C\"}";

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
            fwrite(response_buffer, 1, response_buffer_size, file);
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        ESP_LOGI(TAG, "Response Data: %s", response_buffer);

        if (file != NULL)
        {
            fclose(file);
            file = NULL;
            ESP_LOGI(TAG, "Response Data written to Metadata.txt file successfully");

            // Parse JSON and store directionFiles
            cJSON *json_response = cJSON_ParseWithLength(response_buffer, response_buffer_size);
            if (json_response == NULL)
            {
                ESP_LOGE(TAG, "Failed to parse JSON response");
            }
            else
            {
                cJSON *direction_files_json = cJSON_GetObjectItem(json_response, "directionFiles");
                if (direction_files_json != NULL && cJSON_IsArray(direction_files_json))
                {
                    direction_file_count = cJSON_GetArraySize(direction_files_json);
                    direction_file_names = (char **)malloc(sizeof(char *) * direction_file_count);

                    for (int i = 0; i < direction_file_count; i++)
                    {
                        cJSON *file_name = cJSON_GetArrayItem(direction_files_json, i);
                        if (cJSON_IsString(file_name))
                        {
                            direction_file_names[i] = strdup(file_name->valuestring);
                            ESP_LOGI(TAG, "Stored direction file name %d: %s", i, direction_file_names[i]);
                        }
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to get directionFiles array from JSON response");
                }

                // Extract N values from media array
                cJSON *media_json = cJSON_GetObjectItem(json_response, "media");
                if (media_json != NULL && cJSON_IsArray(media_json))
                {
                    N_count = cJSON_GetArraySize(media_json);
                    N_server = (char **)malloc(sizeof(char *) * N_count);

                    for (int i = 0; i < N_count; i++)
                    {
                        cJSON *media_item = cJSON_GetArrayItem(media_json, i);
                        cJSON *N_value = cJSON_GetObjectItem(media_item, "N");
                        if (cJSON_IsString(N_value))
                        {
                            N_server[i] = strdup(N_value->valuestring);
                            ESP_LOGI(TAG, "Stored N value %d: %s", i, N_server[i]);
                        }
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to get media array from JSON response");
                }

                cJSON_Delete(json_response);
            }
        }

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
    return ESP_OK; // Ensure the function always returns ESP_OK
}

void download_master_json(const char *access_token)
{
    global_access_token = access_token; // Store the access token globally

    esp_http_client_config_t config = {
        .url = DOWNLOAD_URL,
        .event_handler = download_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "x-cubbies-box-token", global_access_token);
    esp_http_client_set_post_field(client, global_request_body, strlen(global_request_body));

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

int get_direction_file_count()
{
    return direction_file_count;
}

const char *get_direction_file_name(int index)
{
    if (index >= 0 && index < direction_file_count)
    {
        return direction_file_names[index];
    }
    return NULL;
}

const char *get_access_token()
{
    return global_access_token;
}

const char *get_request_body()
{
    return global_request_body;
}

int get_N_count()
{
    return N_count;
}

const char *get_N_value(int index)
{
    if (index >= 0 && index < N_count)
    {
        return N_server[index];
    }
    return NULL;
}
