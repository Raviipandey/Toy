#include "player.h"
#include "sdcard.h"
#include "uart.h"
// #include "main.h"

static const char *TAG = "PLAYER_CONTROL";
uint8_t *data;
void player_task(void *pvParameters)
{

    esp_log_level_set("*", ESP_LOG_INFO);
    // UART IDHAR THA

    // Set direction variables (you can modify these based on your logic)
    int N = 1, S = 0, W = 0, E = 0;

    // Read JSON file based on direction
    if (N)
    {
        read_json_file(METADATA_PATH "/north.cubbies");
    }
    else if (S)
    {
        read_json_file(METADATA_PATH "/south.cubbies");
    }
    else if (W)
    {
        read_json_file(METADATA_PATH "/west.cubbies");
    }
    else if (E)
    {
        read_json_file(METADATA_PATH "/east.cubbies");
    }

    // Initialize audio board
    board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, current_volume);

    // Create and configure the audio pipeline
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    fatfs_stream_reader = create_fatfs_stream(AUDIO_STREAM_READER);
    i2s_stream_writer = create_i2s_stream(AUDIO_STREAM_WRITER);
    mp3_decoder = create_mp3_decoder();

    audio_pipeline_register(pipeline, fatfs_stream_reader, "file");
    audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    const char *link_tag[3] = {"file", "mp3", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);

    ESP_LOGI(TAG, "[ 3 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[3.1] Initialize keys on board");
    audio_board_key_init(set);

    // Set up event listener
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    while (current_file_index < file_count)
    {
        play_current_file();

        while (1)
        {
            audio_event_iface_msg_t msg;
            esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Event interface error : %d", ret);
                continue;
            }

            if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)mp3_decoder && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO)
            {
                audio_element_info_t music_info = {0};
                audio_element_getinfo(mp3_decoder, &music_info);
                ESP_LOGI(TAG, "Received music info: sample_rates=%d, bits=%d, ch=%d",
                         music_info.sample_rates, music_info.bits, music_info.channels);

                i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            }

            if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
                msg.source == (void *)fatfs_stream_reader &&
                msg.cmd == AEL_MSG_CMD_REPORT_STATUS &&
                (int)msg.data == AEL_STATUS_STATE_FINISHED)
            {
                ESP_LOGW(TAG, "File finished");
                advance_file_index();
                break;
            }
        }
    }

    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_reset_ringbuffer(pipeline);
    audio_pipeline_reset_elements(pipeline);

    current_file_index++;

    ESP_LOGI(TAG, "Finished playing all files");

    audio_pipeline_terminate(pipeline);
    audio_pipeline_unregister(pipeline, mp3_decoder);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, fatfs_stream_reader);
    audio_pipeline_remove_listener(pipeline);

    audio_event_iface_destroy(evt);
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(mp3_decoder);
    audio_element_deinit(fatfs_stream_reader);

    for (int i = 0; i < file_count; i++)
    {
        free(file_names[i]);
    }
    vTaskDelete(NULL);
}
