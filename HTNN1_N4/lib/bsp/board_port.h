#ifndef BOARD_PORT_H
#define BOARD_PORT_H
#include <stdint.h>
#include "menu.h"

void Board_InitButtons(void);

// Chi goi tu task sau khi scheduler da chay.
void Board_DelayMs(uint32_t ms, void *user);

// Adapter ghi I2C theo dung prototoype Sh1106I2CWrite.
int Board_I2CWrite(uint8_t addr7, const uint8_t *tx,
                   uint16_t len, void *user);

// 0=UP(PB12), 1=DOWN(PB14), 2=SELECT(PB13).
// 1 neu NHAN, 0 neu KHONG NHAN.
uint8_t Board_ReadButton(uint8_t index, void *user);

// Tra ve 0 khi queue nhan yeu cau; chua xac nhan ghi RTC thanh cong.
int Board_WriteRTC(const MenuDateTime *dt, void *user);

// Goi khi nguoi dung luu nguong, co the luu Flash / cap nhat task quạt.
int Board_OnThresholdChanged(MenuLimit id, int32_t value, void *user);

#endif