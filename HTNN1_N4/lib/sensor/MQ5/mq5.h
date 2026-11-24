#ifndef mq5_h
#define mq5_h

#include "stm32f103xb.h"
#include <stdint.h>

void MQ5_Init(void);
float readMQ5();
// Tra mau ADC 12 bit. False neu ADC chua san sang hoac timeout.
bool readMQ5Raw(uint16_t *value);

#endif
