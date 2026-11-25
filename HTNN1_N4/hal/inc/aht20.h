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
/**
 * @brief Gui lenh reset mem cho AHT20 va danh dau can khoi tao lai.
 */
void aht20_reset(void);

/**
 * @brief Chon callback cho ngu trong luc doi AHT20.
 * @param[in] wait_ms Ham cho theo ms. Dung NULL de cho bang DWT.
 */
void aht20_set_delay(void (*wait_ms)(uint32_t));

/**
 * @brief Doc nhiet do va do am tu cung mot lan do AHT20.
 * @param[out] temp Nhiet do theo do C, hoac NAN neu loi. Co the la NULL.
 * @param[out] humi Do am tuong doi theo phan tram, hoac NAN neu loi. Co the la NULL.
 * @pre Task goi co quyen su dung I2C1.
 */
void readAHT20(float *temp, float *humi);

/**
 * @brief Kich mot lan do va lay nhiet do AHT20.
 * @return Nhiet do theo do C, hoac NAN neu loi.
 */
float readAHT20_temp(void);

/**
 * @brief Kich mot lan do va lay do am tuong doi AHT20.
 * @return Do am theo phan tram, hoac NAN neu loi.
 */
float readAHT20_humi(void);

/**
 * @brief Chuan bi I2C1 va kiem tra trang thai hieu chuan cua AHT20.
 * @note Dung aht20_get_last_error() de doc ma loi.
 */
void aht20_init(void);

/**
 * @brief Doc ma loi AHT20 gan nhat.
 * @return Ma thuoc kieu AHT20_Error.
 */
AHT20_Error aht20_get_last_error(void);
#ifdef __cplusplus
}
#endif
#endif
