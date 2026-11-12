#ifndef ADC_H
#define ADC_H

#include "stm32f1xx.h"
#include <stdint.h>

// STM32F103C8: PA0..PA7 hoac PB0..PB1; pin la so chan, khong phai bitmask.
void adc_Config(GPIO_TypeDef *port, uint8_t pin);

#endif
