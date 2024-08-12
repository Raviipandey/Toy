#include "player.h"
#include "sdcard.h"
#include "uart.h"
// #include "main.h"

static const char *TAG = "PLAYER.C";

char *file_names[MAX_FILE_COUNT];
int file_count = 0;
audio_board_handle_t board_handle = NULL;

int current_volume = 75; // Start at 75% volume
int current_file_index = 0;
// uint8_t *data;

audio_element_handle_t fatfs_stream_reader;
audio_element_handle_t i2s_stream_writer;
audio_element_handle_t mp3_decoder;
audio_pipeline_handle_t pipeline;
audio_event_iface_handle_t evt;
void advance_file_index()
{
    current_file_index++;
    if (current_file_index >= file_count)
    {
        current_file_index = 0;
    }
}

void play_current_file()
{
    char file_path[128];
    snprintf(file_path, sizeof(file_path), "%s/%s.mp3", AUDIO_PATH, file_names[current_file_index]);

    ESP_LOGI(TAG, "Playing file: %s", file_path);

    // Get the state of the i2s_stream_writer or any element in the pipeline
    audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);

    // Stop the pipeline if it's already running
    if (el_state == AEL_STATE_RUNNING)
    {
        ESP_LOGI(TAG, "Stopping pipeline before playing the new file");
        audio_pipeline_stop(pipeline);
        audio_pipeline_wait_for_stop(pipeline);
    }

    // Reset the pipeline elements
    audio_pipeline_reset_ringbuffer(pipeline);
    audio_pipeline_reset_elements(pipeline);

    // Set the URI for the new file
    audio_element_set_uri(fatfs_stream_reader, file_path);

    // Start the pipeline
    audio_pipeline_run(pipeline);
}

esp_err_t read_json_file(const char *file_path)
{
    FILE *file = fopen(file_path, "r");
    if (file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file: %s", file_path);
        return ESP_FAIL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(file_size + 1);
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for file content");
        fclose(file);
        return ESP_FAIL;
    }

    size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = '\0';
    fclose(file);

    cJSON *json = cJSON_Parse(buffer);
    free(buffer);

    if (json == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    file_count = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, json)
    {
        if (cJSON_IsString(item) && file_count < MAX_FILE_COUNT)
        {
            file_names[file_count] = strdup(item->valuestring);
            file_count++;
        }
    }

    cJSON_Delete(json);
    return ESP_OK;
}

audio_element_handle_t create_fatfs_stream(audio_stream_type_t type)
{
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = type;
    return fatfs_stream_init(&fatfs_cfg);
}

audio_element_handle_t create_mp3_decoder()
{
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    return mp3_decoder_init(&mp3_cfg);
}

audio_element_handle_t create_i2s_stream(audio_stream_type_t type)
{
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = type;
    return i2s_stream_init(&i2s_cfg);
}





