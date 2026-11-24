#ifndef BMP280_H
#define BMP280_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Dia chi uu tien. Init tu dong thu ca 0x76 va 0x77 (7 bit, khong dich trai).
#ifndef BMP280_I2C_ADDRESS
#define BMP280_I2C_ADDRESS 0x76U
#endif

enum
{
    BMP280_ERROR_NONE = 0,
    BMP280_ERROR_I2C_NACK = 1,
    // Ma 1..4 giu nguyen tu I2C_Error trong i2c.h.
    BMP280_ERROR_CHIP_ID = 5,
    BMP280_ERROR_NOT_INITIALIZED = 6,
    BMP280_ERROR_CALIBRATION = 7,
    BMP280_ERROR_TIMEOUT = 8,
    BMP280_ERROR_INVALID_DATA = 9
};

// Blocking, dung i2c.h va TIM2, khong phu thuoc FreeRTOS.
// Khoi tao I2C, thu ca hai dia chi toi da 3 vong, reset va doc he so hieu chuan.
// Nhiet do x1, ap suat x4, IIR tat; moi lan doc se kich mot phep do.
// Doc tung thanh ghi de tuong thich driver I2C goc; khong can driver trong test.
// Tra 1 khi thanh cong, 0 khi loi. Goi truoc khi cac task dung chung I2C.
uint8_t bmp280_init(void);

// Soft reset va cho NVM san sang; tra 1 thanh cong, 0 khi loi.
// Tu tim dia chi neu chua init; sau reset can init lai truoc khi doc.
uint8_t bmp280_reset(void);

// Tra ap suat da bu nhiet do, don vi Pa; chia 100 de doi sang hPa.
// Tra NAN neu chua init hoac doc loi. Chi goi tu mot luong su dung bus.
float readBmp280(void);
uint8_t bmp280_get_last_error(void);
// Dia chi tim duoc (0 neu chua tim thay); ID cuoi cung doc duoc (0 neu NACK).
uint8_t bmp280_get_address(void);
uint8_t bmp280_get_chip_id(void);

#ifdef __cplusplus
}
#endif

#endif
