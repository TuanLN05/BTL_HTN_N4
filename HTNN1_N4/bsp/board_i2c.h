#ifndef BOARD_I2C_H
#define BOARD_I2C_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phuc hoi bus PB6/PB7 truoc khi goi i2c1_init().
// TIM2 can duoc khoi tao va chay voi buoc dem 1 ms truoc khi goi.
void Board_I2CRecoverBus(void);
// Tra 0 khi bus san sang, -1 neu van bi giu/BUSY.
int Board_I2CInit(void);

// Task phai giu mutex I2C1. Tra false neu NACK, bus loi hoac timeout.
bool Board_I2CWrite(uint8_t address, const uint8_t *data, uint16_t size);
bool Board_I2CRead(uint8_t address, uint8_t *data, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif
