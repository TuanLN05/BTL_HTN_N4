#ifndef mq5_h
#define mq5_h

#include "stm32f103xb.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void MQ5_Init(void);
float readMQ5(void);
// Tra mau ADC 12 bit. False neu ADC chua san sang hoac timeout.
bool readMQ5Raw(uint16_t *value);
#ifdef __cplusplus
}
#endif

#endif
