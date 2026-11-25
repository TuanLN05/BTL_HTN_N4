#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "stm32f103xb.h"

#define BOARD_BUTTON_PORT       GPIOB
#define BOARD_BUTTON_UP_PIN     12U
#define BOARD_BUTTON_DOWN_PIN   14U
#define BOARD_BUTTON_SELECT_PIN 13U

#define BOARD_ALARM_LED_PORT    GPIOA
#define BOARD_ALARM_LED_PIN     1U

// Anh xa ngoai vi co dinh trong cac driver STM32F103, khong remap:
// I2C1: PB6 SCL, PB7 SDA (OLED, AHT20, BMP280, RTC).
// USART1: PA9 TX, PA10 RX; DMA1 Channel 4 TX, Channel 5 RX.
// MQ5: PA0 / ADC1_IN0. Driver ADC DMA dung DMA1 Channel 1.
// TIM2: bo dem noi bo 1 ms cho BSP I2C; SysTick thuoc FreeRTOS.

#endif
