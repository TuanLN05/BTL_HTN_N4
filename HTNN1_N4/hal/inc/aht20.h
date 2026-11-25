#ifndef aht20_h
#define aht20_h

#include "stm32f103xb.h"
#include "hal_i2c.h"

#define aht20_addr 0x38
#define aht20_status 0x71
#define AHT20_SOFTRESET 0xBA

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    AHT20_ERROR_NONE = 0,
    AHT20_ERROR_NACK = 1,
    AHT20_ERROR_I2C_BUS = 2,
    AHT20_ERROR_I2C_TIMEOUT = 3,
    AHT20_ERROR_MEASUREMENT_TIMEOUT = 4,
    AHT20_ERROR_NOT_CALIBRATED = 5,
    AHT20_ERROR_I2C_CONFIG = 6
} AHT20_Error;

// Goi tu mot luong su dung I2C1. Tham so delay tinh bang mili giay.
void aht20_reset(void);
void aht20_set_delay(void (*wait_ms)(uint32_t));
// Ket qua la NAN khi loi. Co the truyen nullptr de bo qua mot gia tri.
void readAHT20(float *temp, float *humi);
float readAHT20_temp(void);
float readAHT20_humi(void);
void aht20_init(void);
AHT20_Error aht20_get_last_error(void);
#ifdef __cplusplus
}
#endif
#endif
