#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include <stdbool.h>

#define BOARD_UART_BAUD 115200U

// Goi truoc khi tao task: khoi tao clock, NVIC, LED, USART1 va TIM2 (1 ms).
void Board_Init(void);

// LED canh bao PA1: HIGH khi bat, LOW khi tat.
#ifdef __cplusplus
extern "C"
#endif
void Board_SetAlarmLed(bool on);

// Thu tu nut: UP, DOWN, SELECT. Muc 1 la dang nhan (active-low).
void Board_ButtonsInit(void);
uint8_t Board_ButtonRead(uint8_t index);

#endif
