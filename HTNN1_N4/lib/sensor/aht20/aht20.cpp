#include "aht20.h"

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

// Task doc AHT20 va giao dien lay du lieu
volatile Sensors_Snapshot sensors_data = {
    {0.0f, 0.0f}, 0U, AHT20_ERROR_NOT_INITIALIZED, 0U
};

// Chi sao chep bo nho trong vung bao ve, khong giao tiep I2C
uint8_t Sensors_GetSnapshot(Sensors_Snapshot *snapshot)
{
    if (snapshot == NULL)
        return 0;

    taskENTER_CRITICAL();
    snapshot->aht20.temperature_c = sensors_data.aht20.temperature_c;
    snapshot->aht20.humidity_percent = sensors_data.aht20.humidity_percent;
    snapshot->aht20_ok = sensors_data.aht20_ok;
    snapshot->aht20_error = sensors_data.aht20_error;
    snapshot->aht20_read_count = sensors_data.aht20_read_count;
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

static void sensors_task(void *argument)
{
    (void)argument;
    uint8_t aht20_ready = 0;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;)
    {
        AHT20_Data aht20_sample;

        // Thu khoi tao lai o chu ky tiep theo neu doc loi
        if (!aht20_ready)
            aht20_ready = AHT20_Init();
        uint8_t aht20_ok = aht20_ready && AHT20_Read(&aht20_sample);
        uint8_t aht20_error = AHT20_GetLastError();
        if (!aht20_ok)
            aht20_ready = 0;

        // Giu mau tot cuoi cung, cap nhat trang thai doc
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

        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000U));
    }
}

BaseType_t Sensors_TaskStart(void)
{
    return xTaskCreate(sensors_task, "AHT20", 384U, NULL,
                       tskIDLE_PRIORITY + 1U, NULL);
}
