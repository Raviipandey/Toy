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
#include "esp_timer.h"

// From inc
// #include "download_master_json.h"
// #include <sdcard.h>
#include "main.h"

static const char *TAG = "PROCESS_AUDIO_FILES";

static FILE *file = NULL;
static int64_t total_download_time = 0;
static int64_t response_body_size = 0; // Add a variable to track the response body size

#define BUFFER_SIZE 4096
static uint8_t download_buffer[BUFFER_SIZE];
static size_t buffer_pos = 0;

static void flush_buffer() {
    if (buffer_pos > 0 && file != NULL) {
        fwrite(download_buffer, 1, buffer_pos, file);
        buffer_pos = 0;
    }
}

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
                size_t remaining = evt->data_len;
                const uint8_t* data = (const uint8_t*)evt->data;
                while (remaining > 0) {
                    size_t to_copy = MIN(remaining, BUFFER_SIZE - buffer_pos);
                    memcpy(download_buffer + buffer_pos, data, to_copy);
                    buffer_pos += to_copy;
                    data += to_copy;
                    remaining -= to_copy;
                    
                    if (buffer_pos == BUFFER_SIZE) {
                        flush_buffer();
                    }
                }
                response_body_size += evt->data_len; // Accumulate the size of the response body
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (file)
            {
                flush_buffer();
                fclose(file);
                file = NULL;
                ESP_LOGI(TAG, "File download complete");
                ESP_LOGI(TAG, "Size of response body: %lld bytes", response_body_size); // Print the size of the response body
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

static void download_file(const char *mp3_file_name)
{
    if (mp3_file_name == NULL)
    {
        ESP_LOGE(TAG, "MP3 file name is NULL");
        return;
    }

    char mp3_url[260];
    snprintf(mp3_url, sizeof(mp3_url), "%s%s.mp3", baseUrl, mp3_file_name);

    // Create temporary file name
    char temp_file_path[256];
    snprintf(temp_file_path, sizeof(temp_file_path), "/sdcard/media/audio/%stemp.mp3", mp3_file_name);

    // Final file name
    char final_file_path[256];
    snprintf(final_file_path, sizeof(final_file_path), "/sdcard/media/audio/%s.mp3", mp3_file_name);

    file = fopen(temp_file_path, "w");
    if (file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", temp_file_path);
        return;
    }

    response_body_size = 0; // Reset response body size before starting the download

    esp_http_client_config_t config = {
        .url = mp3_url,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .buffer_size = 4096,
        .buffer_size_tx = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "x-cubbies-box-token", accessToken);

    // Start the timer
    int64_t start_time = esp_timer_get_time();

    esp_err_t err = esp_http_client_perform(client);

    // Calculate elapsed time
    int64_t elapsed_time = esp_timer_get_time() - start_time;

    // Accumulate the elapsed time to total download time
    total_download_time += elapsed_time;

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "GET request to %s succeeded. Status = %d, content_length = %" PRId64,
                 mp3_url,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
        ESP_LOGI(TAG, "Download time for %s: %lld ms", mp3_file_name, elapsed_time / 1000);
        
        // Rename temp file to final file
        if (rename(temp_file_path, final_file_path) != 0)
        {
            ESP_LOGE(TAG, "Failed to rename file: %s to %s", temp_file_path, final_file_path);
        }
    }
    else
    {
        ESP_LOGE(TAG, "GET request failed: %s", esp_err_to_name(err));
        if (file)
        {
            fclose(file);
            file = NULL;
        }
        // Remove temp file if download failed
        remove(temp_file_path);
    }

    esp_http_client_cleanup(client);
}

void compare_and_update_N_server(const char *path)
{
    int file_count;
    char **file_names = list_files(path, &file_count);

    if (file_names != NULL)
    {
        update_N_server(file_names, file_count);
        free_file_list(file_names, file_count);
    }
}

void process_audio_files(const char *sd_card_path)
{
    // Get the list of files on the SD card
    int sd_file_count;
    char **sd_files = list_files(sd_card_path, &sd_file_count);

    if (sd_files == NULL)
    {
        ESP_LOGE(TAG, "Failed to get list of files from SD card");
        return;
    }

    int n_server_count = N_count;
    ESP_LOGI(TAG, "Total files in N_server: %d", n_server_count);

    // Log duplicate files and create a list of files to download
    ESP_LOGI(TAG, "Duplicate files (already on SD card):");
    vector<string> files_to_download;

    for (int i = 0; i < n_server_count; i++)
    {
        const char *n_server_file = get_N_value(i);
        bool is_duplicate = false;

        for (int j = 0; j < sd_file_count; j++)
        {
            // Compare filenames without extension
            string n_server_name(n_server_file);
            string sd_name(sd_files[j]);
            
            auto n_server_ext = n_server_name.find_last_of('.');
            auto sd_ext = sd_name.find_last_of('.');
            
            if (n_server_ext != string::npos) n_server_name = n_server_name.substr(0, n_server_ext);
            if (sd_ext != string::npos) sd_name = sd_name.substr(0, sd_ext);

            if (n_server_name == sd_name)
            {
                is_duplicate = true;
                ESP_LOGI(TAG, "  %s", n_server_file);
                break;
            }
        }

        if (!is_duplicate)
        {
            files_to_download.push_back(n_server_file);
        }
    }

    ESP_LOGI(TAG, "Processing %d unique audio files", files_to_download.size());

    // Process unique files (not on SD card)
    for (const auto& file_name : files_to_download)
    {
        ESP_LOGI(TAG, "Downloading audio file: %s", file_name.c_str());

        // Send GET request to download the file and save the response data
        download_file(file_name.c_str());
    }

    // Print the total download time
    ESP_LOGI(TAG, "Total download time: %lld ms", total_download_time / 1000);

    // Free the memory allocated for sd_files
    free_file_list(sd_files, sd_file_count);
    free_N_server();
}