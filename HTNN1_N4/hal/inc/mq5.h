#ifndef mq5_h
#define mq5_h

#include "stm32f103xb.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Chuan bi PA0 va hieu chuan ADC1 de doc cam bien MQ5.
 */
void MQ5_Init(void);

/**
 * @brief Doc MQ5 va quy doi mau ADC theo he so 5.5/4095.
 * @return Gia tri quy doi, hoac NAN neu khong doc duoc mau.
 */
float readMQ5(void);

/**
 * @brief Doc mot mau ADC 12 bit tu MQ5.
 * @param[out] value Noi luu gia tri ADC neu doc thanh cong.
 * @retval true Da doc duoc mau.
 * @retval false ADC chua san sang, con tro rong hoac timeout.
 */
bool readMQ5Raw(uint16_t *value);
#ifdef __cplusplus
}
#endif

#endif
