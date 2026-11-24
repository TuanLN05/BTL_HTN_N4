#ifndef ADC_H
#define ADC_H

#include "stm32f1xx.h"
#include <stdint.h>

uint8_t adc1_config(uint8_t channel);

// Doc mot mau bang DMA; goi tuan tu. Tra 0 neu chua init hoac DMA loi/timeout.
// Goi lai adc1_config() sau khi DMA loi/timeout.
uint16_t adc1_read(void);

#endif