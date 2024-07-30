#include "main.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "cJSON.h"

static const char *TAG = "metadata";

static esp_err_t create_file(const char *path, const char *content) {
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", strerror(errno));
        return ESP_FAIL;
    }
    fprintf(file, "%s", content);
    fclose(file);
    return ESP_OK;
}

// Helper function to write an array to a file
static esp_err_t write_array_to_file(const char *file_path, const int *array, int size) {
    FILE *file = fopen(file_path, "w");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", strerror(errno));
        return ESP_FAIL;
    }
    for (int i = 0; i < size; i++) {
        if (i > 0) {
            fprintf(file, ",");
        }
        fprintf(file, "%d", array[i]);
    }
    fclose(file);
    return ESP_OK;
}

void parse_and_store_metadata(const char *response_buffer, size_t response_buffer_size) {
    cJSON *json_response = cJSON_ParseWithLength(response_buffer, response_buffer_size);
    if (json_response == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return;
    }

    // Parse JSON and store baseURL
    cJSON *base_url_json = cJSON_GetObjectItem(json_response, "baseUrl");
    if (base_url_json != NULL && cJSON_IsString(base_url_json)) {
        strncpy(baseUrl, base_url_json->valuestring, sizeof(baseUrl) - 1);
        baseUrl[sizeof(baseUrl) - 1] = '\0';
        ESP_LOGI(TAG, "Stored base URL: %s", baseUrl);
    } else {
        ESP_LOGE(TAG, "Failed to get baseUrl from JSON response");
    }

    // Parse JSON and store directionFiles
    cJSON *direction_files_json = cJSON_GetObjectItem(json_response, "directionFiles");
    if (direction_files_json != NULL && cJSON_IsArray(direction_files_json)) {
        direction_file_count = cJSON_GetArraySize(direction_files_json);
        direction_file_names = (char **)malloc(sizeof(char *) * direction_file_count);

        for (int i = 0; i < direction_file_count; i++) {
            cJSON *file_name = cJSON_GetArrayItem(direction_files_json, i);
            if (cJSON_IsString(file_name)) {
                direction_file_names[i] = strdup(file_name->valuestring);
                ESP_LOGI(TAG, "Stored direction file name %d: %s", i, direction_file_names[i]);
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to get directionFiles array from JSON response");
    }

    // Extract N values from media array
    cJSON *media_json = cJSON_GetObjectItem(json_response, "media");
    if (media_json != NULL && cJSON_IsArray(media_json)) {
        int media_count = cJSON_GetArraySize(media_json);
        N_server = (char **)malloc(sizeof(char *) * media_count * 3); // Allocate space for N, N+C, and N+W

        int server_index = 0; // To track the index in N_server array

        for (int i = 0; i < media_count; i++) {
            cJSON *media_item = cJSON_GetArrayItem(media_json, i);
            cJSON *N_value = cJSON_GetObjectItem(media_item, "N");
            cJSON *T_value = cJSON_GetObjectItem(media_item, "T");

            if (cJSON_IsString(N_value) && cJSON_IsNumber(T_value)) {
                N_server[server_index] = strdup(N_value->valuestring);
                ESP_LOGI(TAG, "Stored N value %d: %s", server_index, N_server[server_index]);
                server_index++;

                if (T_value->valueint == 1) {
                    // Allocate and store N+C
                    size_t len = strlen(N_value->valuestring);
                    N_server[server_index] = (char *)malloc(len + 2); // +1 for 'C' and +1 for '\0'
                    snprintf(N_server[server_index], len + 2, "%sC", N_value->valuestring);
                    ESP_LOGI(TAG, "Stored N+C value %d: %s", server_index, N_server[server_index]);
                    server_index++;

                    // Allocate and store N+W
                    N_server[server_index] = (char *)malloc(len + 2); // +1 for 'W' and +1 for '\0'
                    snprintf(N_server[server_index], len + 2, "%sW", N_value->valuestring);
                    ESP_LOGI(TAG, "Stored N+W value %d: %s", server_index, N_server[server_index]);
                    server_index++;
                }
            }
        }
        N_count = server_index; // Update N_count to reflect the actual number of items stored
        ESP_LOGI(TAG, "Total N_server entries: %d", N_count);
    } else {
        ESP_LOGE(TAG, "Failed to get media array from JSON response");
    }

    // Extract version and save to version.cubbies
    cJSON *version_json = cJSON_GetObjectItem(json_response, "version");
    if (version_json != NULL && cJSON_IsNumber(version_json)) {
        char version_str[16];
        snprintf(version_str, sizeof(version_str), "%d", version_json->valueint);
        if (create_file("/sdcard/media/toys/RFID_1/metadata/version.cubbies", version_str) == ESP_OK) {
            ESP_LOGI(TAG, "version.cubbies file created successfully with content: %s", version_str);
        }
    } else {
        ESP_LOGE(TAG, "Failed to get version from JSON response");
    }

    // Extract productCode and save to productCode.cubbies
    cJSON *product_code_json = cJSON_GetObjectItem(json_response, "productCode");
    if (product_code_json != NULL && cJSON_IsString(product_code_json)) {
        if (create_file("/sdcard/media/toys/RFID_1/metadata/productCode.cubbies", product_code_json->valuestring) == ESP_OK) {
            ESP_LOGI(TAG, "productCode.cubbies file created successfully with content: %s", product_code_json->valuestring);
        }
    } else {
        ESP_LOGE(TAG, "Failed to get productCode from JSON response");
    }

    // Extract recordable and save to recordable.cubbies
    cJSON *recordable_json = cJSON_GetObjectItem(json_response, "recordable");
    if (recordable_json != NULL && cJSON_IsNumber(recordable_json)) {
        char recordable_str[16];
        snprintf(recordable_str, sizeof(recordable_str), "%d", recordable_json->valueint);
        if (create_file("/sdcard/media/toys/RFID_1/metadata/recordable.cubbies", recordable_str) == ESP_OK) {
            ESP_LOGI(TAG, "recordable.cubbies file created successfully with content: %s", recordable_str);
        }
    } else {
        ESP_LOGE(TAG, "Failed to get recordable from JSON response");
    }

    // Extract colourTheme and save to colourTheme.cubbies
    cJSON *colour_theme_json = cJSON_GetObjectItem(json_response, "colourTheme");
    if (colour_theme_json != NULL && cJSON_IsArray(colour_theme_json)) {
        int size = cJSON_GetArraySize(colour_theme_json);
        int *colourThemeArray = (int *)malloc(size * sizeof(int));
        if (colourThemeArray == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for colourTheme array");
        } else {
            for (int i = 0; i < size; i++) {
                colourThemeArray[i] = cJSON_GetArrayItem(colour_theme_json, i)->valueint;
            }
            if (write_array_to_file("/sdcard/media/toys/RFID_1/metadata/colourTheme.cubbies", colourThemeArray, size) == ESP_OK) {
                ESP_LOGI(TAG, "colourTheme.cubbies file created successfully with %d entries", size);
            } else {
                ESP_LOGE(TAG, "Failed to write colourTheme.cubbies");
            }
            free(colourThemeArray);
        }
    } else {
        ESP_LOGE(TAG, "Failed to get colourTheme from JSON response");
    }

    cJSON_Delete(json_response);
}
