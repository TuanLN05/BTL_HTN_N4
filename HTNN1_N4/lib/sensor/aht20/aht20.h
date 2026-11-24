#ifndef aht20_h
#define aht20_h

#include "stm32f103xb.h"
#include "i2c.h"

#define aht20_addr 0x38
#define aht20_status 0x71
#define AHT20_SOFTRESET 0xBA

enum AHT20_Error : uint8_t
{
    AHT20_ERROR_NONE = 0,
    AHT20_ERROR_NACK = 1,
    AHT20_ERROR_I2C_BUS = 2,
    AHT20_ERROR_I2C_TIMEOUT = 3,
    AHT20_ERROR_MEASUREMENT_TIMEOUT = 4,
    AHT20_ERROR_NOT_CALIBRATED = 5,
    AHT20_ERROR_I2C_CONFIG = 6
};

// Goi tu mot luong su dung I2C1. Tham so delay tinh bang mili giay.
void aht20_reset();
void delay(uint32_t time);
// Ket qua la NAN khi loi. Co the truyen nullptr de bo qua mot gia tri.
void readAHT20(float *temp, float *humi);
float readAHT20_temp();
float readAHT20_humi();
void aht20_init();
AHT20_Error aht20_get_last_error();
#endif
