#ifndef I2C_H
#define I2C_H

#include "stm32f1xx.h"
#include <stdint.h>

// 0: Thanh cong
// 1: Slave khong ACK
// 2: Loi bus
// 3: Timeout
// 4: Tham so khong hop le
extern volatile uint8_t I2C_Error;

// Khoi tao I2C va TIM2
void I2C_Init(void);

// Delay bang TIM2
void I2C_DelayMs(uint16_t ms);

// Gui nhieu byte den Slave
void I2C_Write(uint8_t addr, const uint8_t *data, uint16_t len);

// Doc mot byte tu Slave
void I2C_Read(uint8_t addr, uint8_t *data);

// Doc nhieu byte tu Slave
void I2C_ReadBytes(uint8_t addr, uint8_t *data, uint16_t len);

// Ghi mot thanh ghi cua Slave
void I2C_WriteReg8(uint8_t addr, uint8_t reg, uint8_t value);

// Doc mot thanh ghi cua Slave
void I2C_ReadReg8(uint8_t addr, uint8_t reg, uint8_t *data);

// Doc nhieu thanh ghi lien tiep
void I2C_ReadRegBytes(uint8_t addr, uint8_t reg, uint8_t *data,  uint16_t len);

#endif
