#ifndef ADC_H
#define ADC_H

#include "stm32f1xx.h"
#include <stdint.h>

// STM32F103C8: kenh 0..7 -> PA0..PA7; kenh 8..9 -> PB0..PB1.
// Chi cau hinh chan cua kenh duoc chon; tra 1 thanh cong, 0 neu kenh khong hop le.
uint8_t adc1_config(uint8_t channel);

// Doc 1 mau (0..4095) tu kenh da cau hinh gan nhat bang adc1_config
uint16_t adc1_read(void);

#endif
