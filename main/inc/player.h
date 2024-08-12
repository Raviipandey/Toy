#ifndef PLAYER_H
#define PLAYER_H

// #include "main.h"
#include "uart.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "esp_peripherals.h"
#include "periph_touch.h"
#include "periph_adc_button.h"
#include "periph_button.h"
#include "board.h"
#include "esp_vfs_fat.h"
#include "cJSON.h"
#include "fatfs_stream.h"
#include "audio_hal.h"
#include "driver/uart.h"

#define SD_CARD_MOUNT_POINT "/sdcard"
#define METADATA_PATH "/sdcard/media/toys/RFID_1/metadata"
#define AUDIO_PATH "/sdcard/media/audio"



#define MAX_FILE_NAME_LENGTH 32
#define MAX_FILE_COUNT 100
#define MAX_SAFE_VOLUME 100

extern char *file_names[MAX_FILE_COUNT];
extern int file_count;
extern audio_board_handle_t board_handle;

extern int current_volume;
extern int current_file_index;

extern audio_element_handle_t fatfs_stream_reader;
extern audio_element_handle_t i2s_stream_writer;
extern audio_element_handle_t mp3_decoder;
extern audio_pipeline_handle_t pipeline;
extern audio_event_iface_handle_t evt;

#ifdef __cplusplus
extern "C" {
#endif


// Function declarations
esp_err_t read_json_file(const char *file_path);
audio_element_handle_t create_fatfs_stream(audio_stream_type_t type);
audio_element_handle_t create_mp3_decoder(void);
audio_element_handle_t create_i2s_stream(audio_stream_type_t type);
void play_current_file(void);
void advance_file_index(void);
void player_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // PLAYER_H