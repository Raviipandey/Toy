#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "driver/uart.h"
#include "esp_err.h"
// #include "main.h"

// UART configuration
#define UART_PORT_NUM UART_NUM_0
#define UART_BAUD_RATE 115200
#define UART_RXD_PIN 3
#define UART_TXD_PIN 1
#define UART_BUF_SIZE 256

#ifdef __cplusplus
extern "C" {
#endif

void uart_init(void);
void uart_rx_task(void *arg);


#ifdef __cplusplus
}
#endif

#endif // UART_H