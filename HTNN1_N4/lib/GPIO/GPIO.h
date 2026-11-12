#ifndef GPIO_H
#define GPIO_H

#include "stm32f1xx.h"
#include <stdint.h>

#define GPIO_ANALOG        0x0   // Analog
#define GPIO_INPUT         0x4   // Input floating
#define GPIO_INPUT_PULLUP  0x8   // Input pull-up (khong ho tro pull-down)
#define GPIO_OUTPUT_LOW    0x2   // Push-pull 2 MHz, muc ban dau LOW
#define GPIO_OUTPUT_HIGH   0x12  // Push-pull 2 MHz, muc ban dau HIGH
#define GPIO_AF_PP         0xB   // Chuc nang thay the push-pull 50 MHz (vd UART TX)
#define GPIO_AF_OD         0xF   // Chuc nang thay the open-drain 50 MHz (vd I2C SCL/SDA)

uint8_t gpio_Config(GPIO_TypeDef *port, uint8_t pin, uint8_t mode);

uint8_t gpio_Read(GPIO_TypeDef *port, uint8_t pin);   // Tra ve 0 hoac 1
void    gpio_Set(GPIO_TypeDef *port, uint8_t pin);    // muc HIGH
void    gpio_Reset(GPIO_TypeDef *port, uint8_t pin);  // muc LOW

#endif
