#ifndef AHT20_H
#define AHT20_H

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

// Ma 1-6 giu nguyen loi tu I2C_RTOS
enum
{
    AHT20_ERROR_NONE = 0,
    AHT20_ERROR_ARGUMENT = 7,
    AHT20_ERROR_NOT_CALIBRATED = 8,
    AHT20_ERROR_BUSY_TIMEOUT = 9,
    AHT20_ERROR_CRC = 10,
    AHT20_ERROR_INVALID_DATA = 11,
    AHT20_ERROR_NOT_INITIALIZED = 12,
    AHT20_ERROR_TASK_CREATE = 13
};

typedef struct
{
    AHT20_Data aht20;
    uint8_t aht20_ok;
    uint8_t aht20_error;
    uint32_t aht20_read_count;
} Sensors_Snapshot;

// Goi trong main sau I2C_Init va I2C_RTOS_Init, truoc scheduler
// Tao task doc moi giay; 1: tao duoc task, 0: loi; goi lai khong tao trung
// Cam bien duoc khoi tao va doc khi scheduler chay
uint8_t initAHT20(void);

// Tuy chon: gui mau tu task cam bien sang queue man hinh
// Khi loi: data = NULL, valid = 0; khong giu con tro data sau callback
// Callback khong cho lau va khong sua Menu truc tiep
typedef void (*AHT20_UpdateCallback)(const AHT20_Data *data, uint8_t valid, void *user);

// Dang ky truoc initAHT20; tra 0 neu task da duoc tao
uint8_t AHT20_SetUpdateCallback(AHT20_UpdateCallback callback, void *user);

// Init va Read chi goi tu mot task sau khi bo lap lich chay
// Tra ve 1 khi thanh cong, 0 khi loi; Read giu dau ra cu neu loi
uint8_t AHT20_Init(void);
uint8_t AHT20_Read(AHT20_Data *data);
uint8_t AHT20_GetLastError(void);

// Tao task doc AHT20 moi giay, goi sau I2C_RTOS_Init
BaseType_t Sensors_TaskStart(void);

// Lay mau va trang thai, tra 0 neu con tro NULL
uint8_t Sensors_GetSnapshot(Sensors_Snapshot *snapshot);
extern volatile Sensors_Snapshot sensors_data;

// Giao dien nhom, lay mau da luu; tra NAN neu chua co mau hoac doc loi
float readAHT20(void);         // Nhiet do AHT20, do C
float readAHT20Humidity(void); // Do am AHT20, %RH

#ifdef __cplusplus
}
#endif

#endif
