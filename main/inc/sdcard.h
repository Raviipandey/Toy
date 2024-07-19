#ifndef SD_CARD_H
#define SD_CARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

// Function to initialize the SD card
esp_err_t init_sd_card(void);

#ifdef __cplusplus
}
#endif

#endif // SD_CARD_H