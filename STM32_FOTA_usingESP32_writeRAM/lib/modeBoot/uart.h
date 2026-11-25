#ifndef uart_h
#define uart_h

#define UART_BAUD_RATE 115200

#define ESP32_RX        16
#define ESP32_TX        17 

#define uartBoot() Serial2.begin(UART_BAUD_RATE, SERIAL_8E1, ESP32_RX, ESP32_TX)
#define uartApp() Serial2.begin(UART_BAUD_RATE, SERIAL_8N1, ESP32_RX, ESP32_TX)
#endif