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

/**
 * @brief Tim BMP280, reset va nap he so hieu chuan cua cam bien.
 * @retval 1 Khoi tao thanh cong.
 * @retval 0 Khong tim thay cam bien hoac khoi tao loi.
 */
uint8_t bmp280_init(void);

/**
 * @brief Reset mem BMP280 va cho du lieu noi bo san sang.
 * @retval 1 Reset thanh cong.
 * @retval 0 Khong tim thay cam bien hoac reset loi.
 * @note Goi lai bmp280_init() truoc khi do.
 */
uint8_t bmp280_reset(void); // Goi lai init sau reset.

/**
 * @brief Kich mot lan do va lay ap suat da bu nhiet do.
 * @return Ap suat theo Pa, hoac NAN neu loi.
 */
float readBmp280(void);

/**
 * @brief Do ap suat va nhiet do tu cung mot lan do BMP280.
 * @param[out] temperature_c Nhiet do theo do C, hoac NAN neu loi. Co the la NULL.
 * @return Ap suat da bu nhiet do theo Pa, hoac NAN neu loi.
 */
float readBmp280Sample(float *temperature_c);

/**
 * @brief Doc ma loi BMP280 gan nhat.
 * @return Ma BMP280_ERROR_*.
 */
uint8_t bmp280_get_last_error(void);

/**
 * @brief Doc dia chi 7 bit cua BMP280 da nhan dien.
 * @return Dia chi 0x76 hoac 0x77, hoac 0 neu chua xac dinh.
 */
uint8_t bmp280_get_address(void); // 0 neu chua tim thay.
/**
 * @brief Doc ID chip gan nhat nhan duoc khi do tim.
 * @return ID da doc, hoac 0 neu chua doc duoc.
 */
uint8_t bmp280_get_chip_id(void);

/**
 * @brief Chon callback cho ngu trong luc doi BMP280.
 * @param[in] wait_ms Ham cho theo ms. Dung NULL de cho bang DWT.
 */
void bmp280_set_delay(void (*wait_ms)(uint32_t));

/**
 * @brief Cho trong driver BMP280 bang callback hoac bo dem DWT.
 * @param[in] ms So mili giay can cho.
 */
void I2C_DelayMs(uint16_t ms);

#ifdef __cplusplus
}
#endif

#endif
