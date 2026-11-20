#include "aht20_bmp280.h"

extern "C" {
#include "i2c_rtos.h"
#include "FreeRTOS.h"
#include "task.h"
}

#include <math.h>
#include <stddef.h>

// Doc nhiet do va do am AHT20
#define AHT20_STATUS_BUSY 0x80U
#define AHT20_STATUS_CALIBRATED 0x08U
#define AHT20_BUSY_POLL_LIMIT 21U

static uint8_t aht20_initialized;
static uint8_t aht20_last_error = AHT20_ERROR_NOT_INITIALIZED;

// Them mot tick de thoi gian cho khong ngan hon yeu cau
static void delay_ms(uint32_t milliseconds)
{
    vTaskDelay(pdMS_TO_TICKS(milliseconds) + 1U);
}

// Giu ma loi I2C va yeu cau khoi tao lai khi mat lien lac
static uint8_t aht20_transfer_ok(uint8_t error)
{
    aht20_last_error = error;
    if (error != AHT20_ERROR_NONE)
        aht20_initialized = 0;
    return error == AHT20_ERROR_NONE;
}

// Doc trang thai, khong giu mutex trong luc cho cam bien
static uint8_t aht20_read_status(uint8_t *status)
{
    const uint8_t command = 0x71U;
    if (!aht20_transfer_ok(I2C_RTOS_Write(AHT20_I2C_ADDRESS, &command, 1U)))
        return 0;
    return aht20_transfer_ok(I2C_RTOS_ReadBytes(AHT20_I2C_ADDRESS, status, 1U));
}

// Kiem tra toi da 21 lan, cach nhau it nhat 10 ms
static uint8_t aht20_wait_ready(uint8_t *status)
{
    for (uint8_t attempt = 0; attempt < AHT20_BUSY_POLL_LIMIT; ++attempt)
    {
        if (!aht20_read_status(status))
            return 0;
        if ((*status & AHT20_STATUS_BUSY) == 0U)
            return 1;
        if (attempt + 1U < AHT20_BUSY_POLL_LIMIT)
            delay_ms(10U);
    }

    aht20_initialized = 0;
    aht20_last_error = AHT20_ERROR_BUSY_TIMEOUT;
    return 0;
}

// CRC-8: da thuc 0x31, gia tri dau 0xFF
static uint8_t aht20_crc8(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0xFFU;
    for (uint8_t index = 0; index < length; ++index)
    {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8U; ++bit)
            crc = (uint8_t)((crc & 0x80U) ? ((uint32_t)crc << 1) ^ 0x31U : (uint32_t)crc << 1);
    }
    return crc;
}

// Kiem tra hieu chuan sau khi cam bien on dinh nguon
uint8_t AHT20_Init(void)
{
    uint8_t status;
    const uint8_t command[] = {0xBEU, 0x08U, 0x00U};

    aht20_initialized = 0;
    delay_ms(100U);
    if (!aht20_wait_ready(&status))
        return 0;

    if ((status & AHT20_STATUS_CALIBRATED) == 0U)
    {
        if (!aht20_transfer_ok(I2C_RTOS_Write(AHT20_I2C_ADDRESS, command, sizeof(command))))
            return 0;
        delay_ms(10U);
        if (!aht20_wait_ready(&status))
            return 0;
    }

    if ((status & AHT20_STATUS_CALIBRATED) == 0U)
    {
        aht20_last_error = AHT20_ERROR_NOT_CALIBRATED;
        return 0;
    }

    aht20_initialized = 1;
    aht20_last_error = AHT20_ERROR_NONE;
    return 1;
}

// Do moi, kiem tra CRC va doi du lieu 20 bit sang don vi thuc
uint8_t AHT20_Read(AHT20_Data *data)
{
    uint8_t raw[7];
    AHT20_Data sample;
    const uint8_t command[] = {0xACU, 0x33U, 0x00U};

    if (data == NULL)
    {
        aht20_last_error = AHT20_ERROR_ARGUMENT;
        return 0;
    }
    if (!aht20_initialized)
    {
        aht20_last_error = AHT20_ERROR_NOT_INITIALIZED;
        return 0;
    }

    if (!aht20_transfer_ok(I2C_RTOS_Write(AHT20_I2C_ADDRESS, command, sizeof(command))))
        return 0;
    delay_ms(80U);

    // Doc thang khung do, khong chen lenh 0x71 sau khi kich do
    for (uint8_t attempt = 0; attempt < AHT20_BUSY_POLL_LIMIT; ++attempt)
    {
        if (!aht20_transfer_ok(I2C_RTOS_ReadBytes(AHT20_I2C_ADDRESS, raw, sizeof(raw))))
            return 0;
        if ((raw[0] & AHT20_STATUS_BUSY) == 0U)
            break;
        if (attempt + 1U < AHT20_BUSY_POLL_LIMIT)
            delay_ms(10U);
    }

    if (raw[0] & AHT20_STATUS_BUSY)
    {
        aht20_initialized = 0;
        aht20_last_error = AHT20_ERROR_BUSY_TIMEOUT;
        return 0;
    }
    if (aht20_crc8(raw, 6U) != raw[6])
    {
        aht20_last_error = AHT20_ERROR_CRC;
        return 0;
    }
    if ((raw[0] & AHT20_STATUS_CALIBRATED) == 0U)
    {
        aht20_initialized = 0;
        aht20_last_error = AHT20_ERROR_NOT_CALIBRATED;
        return 0;
    }

    const uint32_t humidity = ((uint32_t)raw[1] << 12) |
                              ((uint32_t)raw[2] << 4) | (raw[3] >> 4);
    const uint32_t temperature = ((uint32_t)(raw[3] & 0x0FU) << 16) |
                                 ((uint32_t)raw[4] << 8) | raw[5];
    sample.humidity_percent = (float)humidity * (100.0f / 1048576.0f);
    sample.temperature_c = (float)temperature * (200.0f / 1048576.0f) - 50.0f;
    if (sample.temperature_c < -40.0f || sample.temperature_c > 85.0f ||
        sample.humidity_percent < 0.0f || sample.humidity_percent > 100.0f)
    {
        aht20_last_error = AHT20_ERROR_INVALID_DATA;
        return 0;
    }

    *data = sample;
    aht20_last_error = AHT20_ERROR_NONE;
    return 1;
}

uint8_t AHT20_GetLastError(void)
{
    return aht20_last_error;
}

// Doc nhiet do va ap suat BMP280
typedef struct
{
    uint16_t t1;
    int16_t t2, t3;
    uint16_t p1;
    int16_t p2, p3, p4, p5, p6, p7, p8, p9;
} BMP280_Calibration;

static BMP280_Calibration bmp280_calibration;
static uint8_t bmp280_address;
static uint8_t bmp280_initialized;
static uint8_t bmp280_last_error = BMP280_ERROR_NOT_INITIALIZED;
static volatile float bmp280_pressure_cache_hpa = NAN;

// Bien debug, xem bang Live Expressions hoac in ra man hinh
volatile uint8_t  bmp_dbg_raw[6];
volatile uint32_t bmp_dbg_adc_t;
volatile uint32_t bmp_dbg_adc_p;
volatile float    bmp_dbg_t;
volatile float    bmp_dbg_p;

// Bao ve gia tri ap suat dung chung voi task man hinh
static void bmp280_cache_pressure(float pressure_hpa)
{
    taskENTER_CRITICAL();
    bmp280_pressure_cache_hpa = pressure_hpa;
    taskEXIT_CRITICAL();
}

static uint8_t bmp280_read_failed(uint8_t error)
{
    bmp280_last_error = error;
    bmp280_cache_pressure(NAN);
    return 0;
}

static uint16_t bmp280_read_u16(const uint8_t *raw)
{
    return (uint16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
}

static int16_t bmp280_read_s16(const uint8_t *raw)
{
    uint16_t value = bmp280_read_u16(raw);
    return value < 0x8000U ? (int16_t)value : (int16_t)((int32_t)value - 65536);
}

static uint8_t bmp280_write_register(uint8_t reg, uint8_t value)
{
    const uint8_t raw[] = {reg, value};
    bmp280_last_error = I2C_RTOS_Write(bmp280_address, raw, sizeof(raw));
    return bmp280_last_error == BMP280_ERROR_NONE;
}

// 1: doc tung byte mot (chi dung de kiem tra loi doc nhieu byte cua driver I2C)
// 0: doc ca khoi trong mot giao dich I2C
#ifndef BMP280_SLOW_READ
#define BMP280_SLOW_READ 0
#endif

static uint8_t bmp280_read_block(uint8_t reg, uint8_t *buf, uint8_t len)
{
#if BMP280_SLOW_READ
    for (uint8_t i = 0; i < len; ++i)
    {
        bmp280_last_error = I2C_RTOS_ReadRegBytes(bmp280_address, (uint8_t)(reg + i), &buf[i], 1U);
        if (bmp280_last_error != BMP280_ERROR_NONE)
            return 0;
    }
    return 1;
#else
    bmp280_last_error = I2C_RTOS_ReadRegBytes(bmp280_address, reg, buf, len);
    return bmp280_last_error == BMP280_ERROR_NONE;
#endif
}

// Cho bit trang thai ve 0, toi da 100 ms
static uint8_t bmp280_wait_status(uint8_t mask)
{
    TickType_t start = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(100U) > 0U ?
                               pdMS_TO_TICKS(100U) : 1U;
    uint8_t status;

    for (;;)
    {
        bmp280_last_error = I2C_RTOS_ReadRegBytes(bmp280_address, 0xF3U, &status, 1U);
        if (bmp280_last_error != BMP280_ERROR_NONE)
            return 0;
        if ((status & mask) == 0U)
            return 1;
        if ((TickType_t)(xTaskGetTickCount() - start) >= timeout)
        {
            bmp280_last_error = BMP280_ERROR_TIMEOUT;
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1U) + 1U);
    }
}

// Tim BMP280 tai 0x76 hoac 0x77, khong nhan nham BME280
static uint8_t bmp280_probe_address(void)
{
    static const uint8_t candidates[] = {0x76U, 0x77U};
    uint8_t saw_other_id = 0;
    uint8_t id;

    for (uint8_t i = 0; i < sizeof(candidates); ++i)
    {
        bmp280_last_error = I2C_RTOS_ReadRegBytes(candidates[i], 0xD0U, &id, 1U);
        if (bmp280_last_error != BMP280_ERROR_NONE)
            continue;
        if (id == 0x58U)
        {
            bmp280_address = candidates[i];
            return 1;
        }
        saw_other_id = 1;
    }

    if (saw_other_id)
        bmp280_last_error = BMP280_ERROR_CHIP_ID;
    return 0;
}

// Doc he so bu nhiet do va ap suat tu bo nho cam bien
static uint8_t bmp280_read_calibration(void)
{
    uint8_t raw[24];
    uint8_t all_zero = 1;
    uint8_t all_ff = 1;

    if (!bmp280_read_block(0x88U, raw, sizeof(raw)))
        return 0;

    for (uint8_t i = 0; i < sizeof(raw); ++i)
    {
        if (raw[i] != 0U)
            all_zero = 0;
        if (raw[i] != 0xFFU)
            all_ff = 0;
    }
    if (all_zero || all_ff || bmp280_read_u16(raw + 6) == 0U)
    {
        bmp280_last_error = BMP280_ERROR_CALIBRATION;
        return 0;
    }

    bmp280_calibration.t1 = bmp280_read_u16(raw);
    bmp280_calibration.t2 = bmp280_read_s16(raw + 2);
    bmp280_calibration.t3 = bmp280_read_s16(raw + 4);
    bmp280_calibration.p1 = bmp280_read_u16(raw + 6);
    bmp280_calibration.p2 = bmp280_read_s16(raw + 8);
    bmp280_calibration.p3 = bmp280_read_s16(raw + 10);
    bmp280_calibration.p4 = bmp280_read_s16(raw + 12);
    bmp280_calibration.p5 = bmp280_read_s16(raw + 14);
    bmp280_calibration.p6 = bmp280_read_s16(raw + 16);
    bmp280_calibration.p7 = bmp280_read_s16(raw + 18);
    bmp280_calibration.p8 = bmp280_read_s16(raw + 20);
    bmp280_calibration.p9 = bmp280_read_s16(raw + 22);
    return 1;
}

// Cong thuc bu double cua Bosch. Nhiet do tra ve do C, ap suat tra ve Pa
// Tra ve 0 neu var1 bang 0 hoac khong huu han (khong tinh duoc ap suat)
static uint8_t bmp280_compensate(const BMP280_Calibration *c,
                          uint32_t adc_t, uint32_t adc_p,
                          double *temperature, double *pressure)
{
    double var1, var2, fine, p;

    // Bu nhiet do
    var1 = ((double)adc_t / 16384.0 - c->t1 / 1024.0) * c->t2;
    var2 = (double)adc_t / 131072.0 - c->t1 / 8192.0;
    var2 = var2 * var2 * c->t3;
    fine = var1 + var2;
    *temperature = fine / 5120.0;

    // Bu ap suat
    var1 = (double)(int32_t)fine / 2.0 - 64000.0;
    var2 = var1 * var1 * c->p6 / 32768.0;
    var2 += var1 * c->p5 * 2.0;
    var2 = var2 / 4.0 + c->p4 * 65536.0;
    var1 = (c->p3 * var1 * var1 / 524288.0 + c->p2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * c->p1;
    if (var1 == 0.0 || !isfinite(var1))
        return 0;
    p = (1048576.0 - adc_p - var2 / 4096.0) * 6250.0 / var1;
    var1 = c->p9 * p * p / 2147483648.0;
    var2 = p * c->p8 / 32768.0;
    p += (var1 + var2 + c->p7) / 16.0;
    *pressure = p;
    return 1;
}

uint8_t BMP280_Init(void)
{
    bmp280_initialized = 0;
    bmp280_address = 0;
    bmp280_cache_pressure(NAN);

    if (!bmp280_probe_address() || !bmp280_write_register(0xE0U, 0xB6U))
        return 0;

    delay_ms(2U);
    if (!bmp280_wait_status(0x01U) || !bmp280_read_calibration())
        return 0;

    // Tat bo loc, lay mau nhiet do va ap suat 1 lan, giu che do ngu
    if (!bmp280_write_register(0xF5U, 0x00U) || !bmp280_write_register(0xF4U, 0x24U))
        return 0;

    bmp280_initialized = 1;
    bmp280_last_error = BMP280_ERROR_NONE;
    return 1;
}

uint8_t BMP280_Read(BMP280_Data *data)
{
    uint8_t raw[6];
    uint32_t adc_temperature, adc_pressure;
    double temperature, pressure;

    if (data == NULL)
        return bmp280_read_failed(BMP280_ERROR_ARGUMENT);
    if (!bmp280_initialized)
        return bmp280_read_failed(BMP280_ERROR_NOT_INITIALIZED);

    // Kich mot lan do, cho du thoi gian chuyen doi
    if (!bmp280_write_register(0xF4U, 0x25U))
        return bmp280_read_failed(bmp280_last_error);
    delay_ms(10U);
    if (!bmp280_wait_status(0x08U))
        return bmp280_read_failed(bmp280_last_error);

    // Doc cung luc ap suat va nhiet do cua mot lan do
    if (!bmp280_read_block(0xF7U, raw, sizeof(raw)))
        return bmp280_read_failed(bmp280_last_error);
    for (uint8_t i = 0; i < sizeof(raw); ++i)
        bmp_dbg_raw[i] = raw[i];

    adc_pressure = ((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) |
                   ((uint32_t)raw[2] >> 4);
    adc_temperature = ((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) |
                      ((uint32_t)raw[5] >> 4);
    bmp_dbg_adc_p = adc_pressure;
    bmp_dbg_adc_t = adc_temperature;

    // 0x80000 la gia tri reset: cam bien chua do hoac khong do
    if (adc_temperature == 0x80000U || adc_pressure == 0x80000U)
        return bmp280_read_failed(BMP280_ERROR_RAW_SKIPPED);

    if (!bmp280_compensate(&bmp280_calibration, adc_temperature, adc_pressure,
                    &temperature, &pressure))
        return bmp280_read_failed(BMP280_ERROR_MATH);

    // Luu gia tri tinh duoc de debug, ke ca khi bi loi RANGE
    bmp_dbg_t = (float)temperature;
    bmp_dbg_p = (float)(pressure / 100.0);

    if (!isfinite(temperature) || !isfinite(pressure) ||
        temperature < -40.0 || temperature > 85.0 ||
        pressure < 30000.0 || pressure > 110000.0)
        return bmp280_read_failed(BMP280_ERROR_RANGE);

    data->temperature_c = (float)temperature;
    data->pressure_hpa = (float)(pressure / 100.0);
    bmp280_last_error = BMP280_ERROR_NONE;
    bmp280_cache_pressure(data->pressure_hpa);
    return 1;
}

// Giao dien nhom, chi lay mau da luu, khong giao tiep I2C
float readBMP280(void)
{
    float pressure_hpa;
    taskENTER_CRITICAL();
    pressure_hpa = bmp280_pressure_cache_hpa;
    taskEXIT_CRITICAL();
    return pressure_hpa;
}

uint8_t BMP280_GetLastError(void)
{
    return bmp280_last_error;
}

uint8_t BMP280_GetAddress(void)
{
    return bmp280_address;
}

void BMP280_GetCalibrationDebug(int32_t out[12])
{
    if (out == NULL)
        return;

    out[0]  = bmp280_calibration.t1;
    out[1]  = bmp280_calibration.t2;
    out[2]  = bmp280_calibration.t3;
    out[3]  = bmp280_calibration.p1;
    out[4]  = bmp280_calibration.p2;
    out[5]  = bmp280_calibration.p3;
    out[6]  = bmp280_calibration.p4;
    out[7]  = bmp280_calibration.p5;
    out[8]  = bmp280_calibration.p6;
    out[9]  = bmp280_calibration.p7;
    out[10] = bmp280_calibration.p8;
    out[11] = bmp280_calibration.p9;
}

// Tu kiem tra cong thuc bu tren MCU bang du lieu mau trong datasheet Bosch
// Khong can cam bien. Ket qua dung: 25.08 C va 1006.53 hPa
uint8_t BMP280_SelfTest(float *temperature_c, float *pressure_hpa)
{
    if (temperature_c == NULL || pressure_hpa == NULL)
        return 0;

    static const BMP280_Calibration example =
    {
        27504U, 26435, -1000,
        36477U, -10685, 3024, 2855, 140, -7, 15500, -14600, 6000
    };
    double temperature, pressure;

    if (!bmp280_compensate(&example, 519888U, 415148U, &temperature, &pressure))
        return 0;
    *temperature_c = (float)temperature;
    *pressure_hpa = (float)(pressure / 100.0);
    return 1;
}

// Kich thuoc kieu double tren MCU, don vi byte
uint8_t BMP280_DoubleSize(void)
{
    return (uint8_t)sizeof(double);
}

// Task doc chung va giao dien lay du lieu
volatile Sensors_Snapshot sensors_data = {
    {0.0f, 0.0f}, {0.0f, 0.0f},
    0U, 0U, AHT20_ERROR_NOT_INITIALIZED, BMP280_ERROR_NOT_INITIALIZED,
    0U, 0U
};

// Chi sao chep bo nho trong vung bao ve, khong giao tiep I2C
uint8_t Sensors_GetSnapshot(Sensors_Snapshot *snapshot)
{
    if (snapshot == NULL)
        return 0;

    taskENTER_CRITICAL();
    snapshot->aht20.temperature_c = sensors_data.aht20.temperature_c;
    snapshot->aht20.humidity_percent = sensors_data.aht20.humidity_percent;
    snapshot->bmp280.temperature_c = sensors_data.bmp280.temperature_c;
    snapshot->bmp280.pressure_hpa = sensors_data.bmp280.pressure_hpa;
    snapshot->aht20_ok = sensors_data.aht20_ok;
    snapshot->bmp280_ok = sensors_data.bmp280_ok;
    snapshot->aht20_error = sensors_data.aht20_error;
    snapshot->bmp280_error = sensors_data.bmp280_error;
    snapshot->aht20_read_count = sensors_data.aht20_read_count;
    snapshot->bmp280_read_count = sensors_data.bmp280_read_count;
    taskEXIT_CRITICAL();
    return 1;
}

float readAHT20(void)
{
    Sensors_Snapshot snapshot;
    Sensors_GetSnapshot(&snapshot);
    return snapshot.aht20_ok ? snapshot.aht20.temperature_c : NAN;
}

float readAHT20Humidity(void)
{
    Sensors_Snapshot snapshot;
    Sensors_GetSnapshot(&snapshot);
    return snapshot.aht20_ok ? snapshot.aht20.humidity_percent : NAN;
}

float readBMP280Temperature(void)
{
    Sensors_Snapshot snapshot;
    Sensors_GetSnapshot(&snapshot);
    return snapshot.bmp280_ok ? snapshot.bmp280.temperature_c : NAN;
}

static void sensors_task(void *argument)
{
    (void)argument;
    uint8_t aht20_ready = 0;
    uint8_t bmp280_ready = 0;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;)
    {
        AHT20_Data aht20_sample;
        BMP280_Data bmp280_sample;

        // Thu khoi tao lai o chu ky tiep theo neu doc loi
        if (!aht20_ready)
            aht20_ready = AHT20_Init();
        uint8_t aht20_ok = aht20_ready && AHT20_Read(&aht20_sample);
        uint8_t aht20_error = AHT20_GetLastError();
        if (!aht20_ok)
            aht20_ready = 0;

        // Giu mau tot cuoi cung, cap nhat trang thai rieng tung cam bien
        taskENTER_CRITICAL();
        if (aht20_ok)
        {
            sensors_data.aht20.temperature_c = aht20_sample.temperature_c;
            sensors_data.aht20.humidity_percent = aht20_sample.humidity_percent;
            ++sensors_data.aht20_read_count;
        }
        sensors_data.aht20_ok = aht20_ok;
        sensors_data.aht20_error = aht20_error;
        taskEXIT_CRITICAL();

        if (!bmp280_ready)
            bmp280_ready = BMP280_Init();
        uint8_t bmp280_ok = bmp280_ready && BMP280_Read(&bmp280_sample);
        uint8_t bmp280_error = BMP280_GetLastError();
        if (!bmp280_ok)
            bmp280_ready = 0;

        taskENTER_CRITICAL();
        if (bmp280_ok)
        {
            sensors_data.bmp280.temperature_c = bmp280_sample.temperature_c;
            sensors_data.bmp280.pressure_hpa = bmp280_sample.pressure_hpa;
            ++sensors_data.bmp280_read_count;
        }
        sensors_data.bmp280_ok = bmp280_ok;
        sensors_data.bmp280_error = bmp280_error;
        taskEXIT_CRITICAL();

        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000U));
    }
}

BaseType_t Sensors_TaskStart(void)
{
    return xTaskCreate(sensors_task, "AHT20_BMP280", 384U, NULL,
                       tskIDLE_PRIORITY + 1U, NULL);
}
