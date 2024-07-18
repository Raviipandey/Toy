#ifndef BOX_LOGIN_H
#define BOX_LOGIN_H

#include "esp_err.h"

// Function to initiate the HTTP POST request for login
void http_post_task(void *pvParameters);

// External declaration of accessToken
extern char accessToken[40];

#endif // BOX_LOGIN_H
