#include "main.h"
#include "player.h"
#include "uart.h"

static const char *TAG = "UART";
static uint8_t *data;
void uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TXD_PIN, UART_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

// UART Task to Handle Commands
void uart_rx_task(void *arg)
{
    data = (uint8_t *)malloc(UART_BUF_SIZE);
    if (data == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for UART buffer");
        vTaskDelete(NULL);
    }

    while (1)
    {
        int rxBytes = uart_read_bytes(UART_PORT_NUM, data, UART_BUF_SIZE - 1, pdMS_TO_TICKS(100));
        if (rxBytes > 0)
        {
            data[rxBytes] = 0; // Null-terminate the string
            ESP_LOGI(TAG, "Received: '%s'", data);

            // Get the current state of the audio pipeline
            audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);

            if (strstr((char *)data, "Next"))
            {
                ESP_LOGI(TAG, "Next command received");
                // Perform the action based on the current state
                if (el_state == AEL_STATE_RUNNING)
                {
                    advance_file_index();
                    play_current_file();
                }
                else
                {
                    ESP_LOGI(TAG, "Next command ignored - Player is paused");
                }
            }
            else if (strstr((char *)data, "Prev"))
            {
                ESP_LOGI(TAG, "Prev command received");
                // Perform the action based on the current state
                if (el_state == AEL_STATE_RUNNING)
                {
                    if (current_file_index > 0)
                    {
                        current_file_index--;
                    }
                    else
                    {
                        current_file_index = file_count - 1;
                    }
                    play_current_file();
                }
                else
                {
                    ESP_LOGI(TAG, "Prev command ignored - Player is paused");
                }
            }
            else if (strstr((char *)data, "Play"))
            {
                ESP_LOGI(TAG, "Play/Pause command received");
                // Logic to toggle play/pause
                if (el_state == AEL_STATE_RUNNING)
                {
                    audio_pipeline_pause(pipeline);
                }
                else if (el_state == AEL_STATE_PAUSED)
                {
                    audio_pipeline_resume(pipeline);
                }
                else if (el_state == AEL_STATE_FINISHED || el_state == AEL_STATE_INIT)
                {
                    audio_pipeline_run(pipeline);
                }
            }
            else if (strstr((char *)data, "Vol Up"))
            {
                ESP_LOGI(TAG, "Volume Up command received");
                // Logic to increase volume
                current_volume += 10;
                if (current_volume > MAX_SAFE_VOLUME)
                {
                    current_volume = MAX_SAFE_VOLUME;
                }
                audio_hal_set_volume(board_handle->audio_hal, current_volume);
            }
            else if (strstr((char *)data, "Vol Down"))
            {
                ESP_LOGI(TAG, "Volume Down command received");
                // Logic to decrease volume
                current_volume -= 10;
                if (current_volume < 0)
                {
                    current_volume = 0;
                }
                audio_hal_set_volume(board_handle->audio_hal, current_volume);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to prevent tight loop
    }

    free(data);
}
