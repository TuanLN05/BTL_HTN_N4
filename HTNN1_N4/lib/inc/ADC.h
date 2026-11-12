#ifndef ADC_H
#define ADC_H

#include "stm32f1xx.h"
#include <stdint.h>

// STM32F103C8: PA0..PA7 hoac PB0..PB1; pin la so chan, khong phai bitmask.
// Tra ve 1 neu cau hinh thanh cong, 0 neu chan khong ho tro ADC.
uint8_t  adc_Config(GPIO_TypeDef *port, uint8_t pin);

// Doc 1 mau (0..4095) tu kenh da cau hinh gan nhat bang adc_Config
uint16_t adc_Read(void);

#endif