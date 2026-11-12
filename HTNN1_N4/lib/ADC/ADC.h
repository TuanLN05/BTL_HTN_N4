#ifndef ADC_H
#define ADC_H

#include "stm32f1xx.h"
#include <stdint.h>

uint8_t adc1_config(uint8_t channel);

uint16_t adc1_read(void);

#endif