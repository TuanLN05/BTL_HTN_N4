#include "aht20.h"

extern "C" {
#include "i2c.h"

// Ham delay TIM2 da co trong i2c.c
void I2C_DelayMs(uint16_t ms);
}

#include <math.h>

static const uint8_t aht20_address = 0x38U;
static const uint8_t busy_mask = 0x80U;
static const uint8_t calibrated_mask = 0x08U;
static const uint8_t poll_limit = 21U;
static uint8_t init_requested;
static uint8_t initialized;

static uint8_t read_status(uint8_t *status)
{
    const uint8_t command = 0x71U;
    I2C_Write(aht20_address, &command, 1U);
    if (I2C_Error != 0U)
        return 0U;

    I2C_ReadBytes(aht20_address, status, 1U);
    return I2C_Error == 0U;
}

static uint8_t wait_ready(uint8_t *status)
{
    for (uint8_t attempt = 0U; attempt < poll_limit; ++attempt)
    {
        if (!read_status(status))
            return 0U;
        if ((*status & busy_mask) == 0U)
            return 1U;
        if (attempt + 1U < poll_limit)
            I2C_DelayMs(10U);
    }
    return 0U;
}

static uint8_t crc8(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0xFFU;
    for (uint8_t index = 0U; index < length; ++index)
    {
        crc ^= data[index];
        for (uint8_t bit = 0U; bit < 8U; ++bit)
            crc = (uint8_t)((crc & 0x80U) ? ((uint32_t)crc << 1) ^ 0x31U : (uint32_t)crc << 1);
    }
    return crc;
}

void initAHT20(void)
{
    uint8_t status;
    const uint8_t command[] = {0xBEU, 0x08U, 0x00U};

    init_requested = 1U;
    initialized = 0U;
    I2C_DelayMs(100U);
    if (!wait_ready(&status))
        return;

    if ((status & calibrated_mask) == 0U)
    {
        I2C_Write(aht20_address, command, sizeof(command));
        if (I2C_Error != 0U)
            return;
        I2C_DelayMs(10U);
        if (!wait_ready(&status))
            return;
    }

    initialized = (status & calibrated_mask) != 0U;
}

static uint8_t read_sample(float *temperature, float *humidity)
{
    uint8_t raw[7] = {0U};
    const uint8_t command[] = {0xACU, 0x33U, 0x00U};

    if (!init_requested)
        return 0U;

    // Chi thu init lai mot lan trong moi lan doc
    if (!initialized)
    {
        initAHT20();
        if (!initialized)
            return 0U;
    }

    I2C_Write(aht20_address, command, sizeof(command));
    if (I2C_Error != 0U)
    {
        initialized = 0U;
        return 0U;
    }
    I2C_DelayMs(80U);

    // Doc nguyen khung do, khong chen lenh trang thai sau khi kich do
    for (uint8_t attempt = 0U; attempt < poll_limit; ++attempt)
    {
        I2C_ReadBytes(aht20_address, raw, sizeof(raw));
        if (I2C_Error != 0U)
        {
            initialized = 0U;
            return 0U;
        }
        if ((raw[0] & busy_mask) == 0U)
            break;
        if (attempt + 1U < poll_limit)
            I2C_DelayMs(10U);
    }

    if ((raw[0] & busy_mask) != 0U || crc8(raw, 6U) != raw[6] ||
        (raw[0] & calibrated_mask) == 0U)
    {
        initialized = 0U;
        return 0U;
    }

    const uint32_t raw_humidity = ((uint32_t)raw[1] << 12) |
                                  ((uint32_t)raw[2] << 4) | (raw[3] >> 4);
    const uint32_t raw_temperature = ((uint32_t)(raw[3] & 0x0FU) << 16) |
                                     ((uint32_t)raw[4] << 8) | raw[5];
    *temperature = (float)raw_temperature * (200.0f / 1048576.0f) - 50.0f;
    *humidity = (float)raw_humidity * (100.0f / 1048576.0f);
    return *temperature >= -40.0f && *temperature <= 85.0f &&
           *humidity >= 0.0f && *humidity <= 100.0f;
}

float readAht20Temp(void)
{
    float temperature, humidity;
    return read_sample(&temperature, &humidity) ? temperature : NAN;
}

float readAht20Humi(void)
{
    float temperature, humidity;
    return read_sample(&temperature, &humidity) ? humidity : NAN;
}
