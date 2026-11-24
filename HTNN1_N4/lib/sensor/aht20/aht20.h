#ifndef aht20_h
#define aht20_h

#include "stm32f103xb.h"
#include "i2c.h"

#define aht20_addr 0x38
#define aht20_status 0x71
#define AHT20_SOFTRESET 0xBA
void aht20_reset();
void delay(uint32_t time);
void readAHT20(float *temp, float *humi);
float readAHT20_temp();
float readAHT20_humi();
void aht20_init();
#endif