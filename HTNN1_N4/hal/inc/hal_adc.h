#ifndef HAL_ADC_H
#define HAL_ADC_H

#include "stm32f1xx.h"
#include <stdint.h>

/**
 * @brief Cau hinh ADC1 lay mot mau qua DMA1 Channel 1.
 * @param[in] channel Kenh ADC1 tu 0 den 9.
 * @retval 1 Cau hinh thanh cong.
 * @retval 0 Kenh khong duoc ho tro.
 */
uint8_t adc1_config(uint8_t channel);

/**
 * @brief Doc mot mau ADC1 bang DMA.
 * @return Mau ADC 12 bit, hoac 0 neu chua khoi tao, DMA loi hay timeout.
 * @note Goi lai adc1_config() sau loi DMA hoac timeout.
 */
uint16_t adc1_read(void);

#endif
