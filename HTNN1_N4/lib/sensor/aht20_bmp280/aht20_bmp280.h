#ifndef AHT20_BMP280_H
#define AHT20_BMP280_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"

#define AHT20_I2C_ADDRESS 0x38U

typedef struct
{
    float temperature_c;
    float humidity_percent;
} AHT20_Data;

typedef struct
{
    float temperature_c;
    float pressure_hpa;
} BMP280_Data;

// Ma 1-6 giu nguyen loi tu I2C_RTOS
enum
{
    AHT20_ERROR_NONE = 0,
    AHT20_ERROR_ARGUMENT = 7,
    AHT20_ERROR_NOT_CALIBRATED = 8,
    AHT20_ERROR_BUSY_TIMEOUT = 9,
    AHT20_ERROR_CRC = 10,
    AHT20_ERROR_INVALID_DATA = 11,
    AHT20_ERROR_NOT_INITIALIZED = 12
};

enum
{
    BMP280_ERROR_NONE = 0,
    BMP280_ERROR_ARGUMENT = 7,
    BMP280_ERROR_CHIP_ID = 8,
    BMP280_ERROR_TIMEOUT = 9,
    BMP280_ERROR_CALIBRATION = 10,
    BMP280_ERROR_INVALID_DATA = 11,
    BMP280_ERROR_NOT_INITIALIZED = 12,
    BMP280_ERROR_RAW_SKIPPED = 13,
    BMP280_ERROR_MATH = 14,
    BMP280_ERROR_RANGE = 15
};

typedef struct
{
    AHT20_Data aht20;
    BMP280_Data bmp280;
    uint8_t aht20_ok;
    uint8_t bmp280_ok;
    uint8_t aht20_error;
    uint8_t bmp280_error;
    uint32_t aht20_read_count;
    uint32_t bmp280_read_count;
} Sensors_Snapshot;

// Init va Read chi goi tu mot task sau khi bo lap lich chay
// Tra ve 1 khi thanh cong, 0 khi loi; Read giu dau ra cu neu loi
uint8_t AHT20_Init(void);
uint8_t AHT20_Read(AHT20_Data *data);
uint8_t AHT20_GetLastError(void);

uint8_t BMP280_Init(void);
uint8_t BMP280_Read(BMP280_Data *data);
uint8_t BMP280_GetLastError(void);
uint8_t BMP280_GetAddress(void);

// Tao task doc hai cam bien moi giay, goi sau I2C_RTOS_Init
BaseType_t Sensors_TaskStart(void);

// Lay mau va trang thai, tra 0 neu con tro NULL
uint8_t Sensors_GetSnapshot(Sensors_Snapshot *snapshot);
extern volatile Sensors_Snapshot sensors_data;

// Giao dien nhom, lay mau da luu; tra NAN neu chua co mau hoac doc loi
float readAHT20(void);             // Nhiet do AHT20, do C
float readAHT20Humidity(void);     // Do am AHT20, %RH
float readBMP280(void);            // Ap suat BMP280, hPa
float readBMP280Temperature(void); // Nhiet do BMP280, do C

// Du lieu BMP280 de xem trong Watch
extern volatile uint8_t bmp_dbg_raw[6];
extern volatile uint32_t bmp_dbg_adc_t;
extern volatile uint32_t bmp_dbg_adc_p;
extern volatile float bmp_dbg_t;
extern volatile float bmp_dbg_p;

// He so theo thu tu T1 T2 T3 P1 P2 P3 P4 P5 P6 P7 P8 P9
void BMP280_GetCalibrationDebug(int32_t out[12]);
uint8_t BMP280_SelfTest(float *temperature_c, float *pressure_hpa);
uint8_t BMP280_DoubleSize(void);

#ifdef __cplusplus
}
#endif

#endif
