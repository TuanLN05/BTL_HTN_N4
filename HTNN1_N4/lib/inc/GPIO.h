#ifndef GPIO_H
#define GPIO_H

#include "stm32f1xx.h"
#include <stdint.h>

#define GPIO_ANALOG       0x0
#define GPIO_INPUT        0x4  // Input floating
#define GPIO_INPUT_PULLUP 0x8
#define GPIO_OUTPUT_LOW   0x2  // Push-pull 2 MHz, muc ban dau LOW
#define GPIO_OUTPUT_HIGH  0x12 // Push-pull 2 MHz, muc ban dau HIGH

// port: GPIOA/GPIOB/GPIOC; pin: so chan 0..15, khong phai bitmask.
void gpio_Config(GPIO_TypeDef *port, uint8_t pin, uint8_t mode);

// Goi gpio_Config() truoc khi doc/ghi; pin van la so chan 0..15.
uint8_t readInputPin(GPIO_TypeDef *port, uint8_t pin); // Tra ve 0 hoac 1
void setPin(GPIO_TypeDef *port, uint8_t pin);         // Dat muc HIGH
void resetPin(GPIO_TypeDef *port, uint8_t pin);       // Dat muc LOW

#endif
