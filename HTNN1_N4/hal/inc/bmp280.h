#ifndef BMP280_H
#define BMP280_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Dia chi thu truoc. Init thu ca 0x76 (SDO = GND) va 0x77 (SDO = VDDIO).
#ifndef BMP280_I2C_ADDRESS
#define BMP280_I2C_ADDRESS 0x76U
#endif

enum
{
    BMP280_ERROR_NONE = 0,
    BMP280_ERROR_I2C_NACK = 1,
    BMP280_ERROR_I2C_BUS = 2,
    BMP280_ERROR_I2C_TIMEOUT = 3,
    BMP280_ERROR_I2C_CONFIG = 4,
    BMP280_ERROR_CHIP_ID = 5,
    BMP280_ERROR_NOT_INITIALIZED = 6,
    BMP280_ERROR_CALIBRATION = 7,
    BMP280_ERROR_TIMEOUT = 8,
    BMP280_ERROR_INVALID_DATA = 9
};

// Goi tu mot luong su dung I2C. Init/reset tra 1 khi thanh cong, 0 khi loi.
// Ma loi 1..4 danh cho giao dich I2C1 cua BMP280.
uint8_t bmp280_init(void);
uint8_t bmp280_reset(void); // Goi lai init sau reset.

// Ap suat da bu nhiet do (Pa), tra NAN khi loi. Chia 100 de doi sang hPa.
float readBmp280(void);
// Cung mot lan do: tra ap suat (Pa) va ghi nhiet do (C) neu co con tro.
float readBmp280Sample(float *temperature_c);
uint8_t bmp280_get_last_error(void);
uint8_t bmp280_get_address(void); // 0 neu chua tim thay.
uint8_t bmp280_get_chip_id(void);

// Ham cho mili giay dung trong driver BMP280. NULL dung DWT nhu mac dinh.
void bmp280_set_delay(void (*wait_ms)(uint32_t));
void I2C_DelayMs(uint16_t ms);

#ifdef __cplusplus
}
#endif

#endif
