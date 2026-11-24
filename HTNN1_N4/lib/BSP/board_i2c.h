#ifndef BOARD_I2C_H
#define BOARD_I2C_H

// Phuc hoi bus PB6/PB7 truoc khi goi i2c1_init().
// TIM2 can duoc khoi tao va chay voi buoc dem 1 ms truoc khi goi.
void Board_I2CRecoverBus(void);
// TIM2 1 ms phai dang chay. Tra 0 neu bus san sang, -1 neu van ket.
int Board_I2CInit(void);

#endif
