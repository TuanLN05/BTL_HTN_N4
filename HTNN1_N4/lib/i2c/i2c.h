#ifndef i2c_h
#define i2c_h

#include "stm32f10x.h"
#include "stdint.h"

void i2c1_init(void);
void i2c1_Start(void);
void i2c1_Stop(void);
void i2c1_writeData(uint8_t data);
void i2c1_readBytes(uint8_t addr, uint8_t *data, uint8_t size);
void i2c1_writeBytes(uint8_t addr, uint8_t *data, uint8_t size);
void i2c1_writeAddr(uint8_t addr);

#endif