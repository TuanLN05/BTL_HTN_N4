#ifndef I2C_RTOS_H
#define I2C_RTOS_H
#include <stdint.h>

// Tao 1 Mutex cho I2C1. Goi truoc vTaskStartScheduler().
uint8_t I2C_RTOS_Init(void);

// Gia tri tra ve: 0=OK, 1..4 loi tu I2C cua nhom,
// 5=khong lay duoc Mutex, 6=Mutex chua khoi tao.
uint8_t I2C_RTOS_Write(uint8_t addr, const uint8_t *data, uint16_t len);
uint8_t I2C_RTOS_ReadBytes(uint8_t addr, uint8_t *data, uint16_t len);
uint8_t I2C_RTOS_ReadRegBytes(uint8_t addr, uint8_t reg,
                              uint8_t *data, uint16_t len);

#endif