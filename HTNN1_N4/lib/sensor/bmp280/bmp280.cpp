#include "bmp280.h"
#include "i2c.h"
#include <math.h>

// Bosch BMP280 datasheet: forced mode, muc 3.6.2 va cong thuc bu muc 3.11.
// https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf
#define BMP280_REG_CALIBRATION 0x88U
#define BMP280_REG_ID          0xD0U
#define BMP280_REG_RESET       0xE0U
#define BMP280_REG_STATUS      0xF3U
#define BMP280_REG_CTRL_MEAS   0xF4U
#define BMP280_REG_CONFIG      0xF5U
#define BMP280_REG_PRESSURE    0xF7U
#define BMP280_CHIP_ID         0x58U
#define BMP280_RESET_COMMAND   0xB6U
#define BMP280_STATUS_NVM      0x01U
#define BMP280_STATUS_MEASURING 0x08U
#define BMP280_CTRL_SLEEP      0x2CU // osrs_t x1, osrs_p x4, sleep.
#define BMP280_CTRL_FORCED     0x2DU // osrs_t x1, osrs_p x4, forced.
#define BMP280_STATUS_POLL_LIMIT 100U
#define BMP280_PROBE_ATTEMPTS    3U

typedef struct
{
    uint16_t t1;
    int16_t t2, t3;
    uint16_t p1;
    int16_t p2, p3, p4, p5, p6, p7, p8, p9;
} BMP280_Calibration;

static BMP280_Calibration calibration;
static uint8_t bus_initialized;
static uint8_t initialized;
static uint8_t last_error = BMP280_ERROR_NOT_INITIALIZED;
static uint8_t active_address = BMP280_I2C_ADDRESS;
static uint8_t detected_address;
static uint8_t detected_chip_id;

static uint8_t bmp280_prepare_bus(void)
{
    if (!bus_initialized)
    {
        I2C_Init();
        last_error = I2C_Error;
        if (last_error != BMP280_ERROR_NONE)
            return 0U;
        bus_initialized = 1U;
        I2C_DelayMs(10U); // Cho nguon on dinh truoc khi thu dia chi.
    }
    return 1U;
}

static uint8_t bmp280_finish_transfer(void)
{
    // Luu loi ngay: driver goc phat STOP nhung khong doi STOP ket thuc.
    last_error = I2C_Error;
    I2C_DelayMs(1U);
    if (last_error != BMP280_ERROR_NONE)
    {
        // Ke ca NACK: lan thu tiep theo can khoi tao lai ngoai vi sau Abort.
        bus_initialized = 0U;
        initialized = 0U;
        return 0U;
    }
    return 1U;
}

static uint8_t bmp280_read_registers(uint8_t reg, uint8_t *data, uint16_t length)
{
    if (!bmp280_prepare_bus())
        return 0U;

    // Tranh nhanh nhan >=3 byte bi loi BTF/STOP trong driver I2C goc.
    // Calibration la hang so. Du lieu do chi doc khi forced mode da ve sleep.
    for (uint16_t index = 0U; index < length; ++index)
    {
        I2C_ReadRegBytes(active_address, (uint8_t)(reg + index), data + index, 1U);
        if (!bmp280_finish_transfer())
            return 0U;
    }
    return 1U;
}

static uint8_t bmp280_write_register(uint8_t reg, uint8_t value)
{
    if (!bmp280_prepare_bus())
        return 0U;
    I2C_WriteReg8(active_address, reg, value);
    return bmp280_finish_transfer();
}

static uint8_t bmp280_wait_status_clear(uint8_t mask)
{
    for (uint16_t attempt = 0U; attempt < BMP280_STATUS_POLL_LIMIT; ++attempt)
    {
        uint8_t status;
        if (!bmp280_read_registers(BMP280_REG_STATUS, &status, 1U))
            return 0U;
        if ((status & mask) == 0U)
            return 1U;
        if (attempt + 1U < BMP280_STATUS_POLL_LIMIT)
            I2C_DelayMs(1U);
    }

    last_error = BMP280_ERROR_TIMEOUT;
    return 0U;
}

static uint16_t bmp280_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint8_t bmp280_find_sensor(void)
{
    detected_address = 0U;
    detected_chip_id = 0U;
    const uint8_t addresses[2] = {
        BMP280_I2C_ADDRESS, BMP280_I2C_ADDRESS == 0x76U ? 0x77U : 0x76U
    };
    uint8_t scan_error = BMP280_ERROR_I2C_NACK;
    for (uint8_t attempt = 0U; attempt < BMP280_PROBE_ATTEMPTS; ++attempt)
    {
        uint8_t wrong_chip = 0U;
        scan_error = BMP280_ERROR_I2C_NACK;
        for (uint8_t index = 0U; index < 2U; ++index)
        {
            active_address = addresses[index];
            uint8_t id;
            if (!bmp280_read_registers(BMP280_REG_ID, &id, 1U))
            {
                scan_error = last_error;
                if (scan_error != BMP280_ERROR_I2C_NACK)
                    break; // Loi bus: khoi tao lai va thu o vong tiep theo.
                continue;
            }
            detected_chip_id = id;
            if (id == BMP280_CHIP_ID)
            {
                detected_address = active_address;
                return 1U;
            }
            wrong_chip = 1U;
        }
        if (wrong_chip && scan_error == BMP280_ERROR_I2C_NACK)
        {
            last_error = BMP280_ERROR_CHIP_ID;
            return 0U;
        }
        // Thu lai ngoai vi neu loi START/BUSY hoac chua nhan ACK.
        bus_initialized = 0U;
    }

    last_error = scan_error;
    return 0U;
}

uint8_t bmp280_reset(void)
{
    initialized = 0U;
    // Cho phep reset truoc init, ke ca khi module dung dia chi 0x77.
    if (detected_address == 0U && !bmp280_find_sensor())
        return 0U;
    active_address = detected_address;
    if (!bmp280_write_register(BMP280_REG_RESET, BMP280_RESET_COMMAND))
        return 0U;

    I2C_DelayMs(3U);
    return bmp280_wait_status_clear(BMP280_STATUS_NVM);
}

uint8_t bmp280_init(void)
{
    uint8_t raw[24];
    initialized = 0U;
    if (!bmp280_find_sensor())
        return 0U;
    if (!bmp280_reset())
        return 0U;
    if (!bmp280_read_registers(BMP280_REG_CALIBRATION, raw, sizeof(raw)))
        return 0U;

    // He so little-endian; chi T1 va P1 la so khong dau.
    calibration.t1 = bmp280_u16_le(raw);
    calibration.t2 = (int16_t)bmp280_u16_le(raw + 2);
    calibration.t3 = (int16_t)bmp280_u16_le(raw + 4);
    calibration.p1 = bmp280_u16_le(raw + 6);
    calibration.p2 = (int16_t)bmp280_u16_le(raw + 8);
    calibration.p3 = (int16_t)bmp280_u16_le(raw + 10);
    calibration.p4 = (int16_t)bmp280_u16_le(raw + 12);
    calibration.p5 = (int16_t)bmp280_u16_le(raw + 14);
    calibration.p6 = (int16_t)bmp280_u16_le(raw + 16);
    calibration.p7 = (int16_t)bmp280_u16_le(raw + 18);
    calibration.p8 = (int16_t)bmp280_u16_le(raw + 20);
    calibration.p9 = (int16_t)bmp280_u16_le(raw + 22);

    uint8_t all_ff = 1U;
    for (uint8_t index = 0U; index < sizeof(raw); ++index)
        all_ff &= raw[index] == 0xFFU;
    if (calibration.p1 == 0U || all_ff)
    {
        last_error = BMP280_ERROR_CALIBRATION;
        return 0U;
    }

    // Reset dua cam bien ve sleep, cho phep ghi config an toan.
    if (!bmp280_write_register(BMP280_REG_CONFIG, 0x00U) ||
        !bmp280_write_register(BMP280_REG_CTRL_MEAS, BMP280_CTRL_SLEEP))
        return 0U;

    uint8_t control;
    if (!bmp280_read_registers(BMP280_REG_CTRL_MEAS, &control, 1U))
        return 0U;
    if (control != BMP280_CTRL_SLEEP)
    {
        last_error = BMP280_ERROR_INVALID_DATA;
        return 0U;
    }

    initialized = 1U;
    return 1U;
}

static float bmp280_compensate_pressure(int32_t adc_pressure, int32_t adc_temperature)
{
    // Bu nhiet do truoc de tinh t_fine cho chinh mau ap suat nay.
    const int32_t delta1 = (adc_temperature >> 3) - (int32_t)calibration.t1 * 2;
    const int32_t delta2 = (adc_temperature >> 4) - (int32_t)calibration.t1;
    const int32_t temp1 = (int32_t)(((int64_t)delta1 * calibration.t2) >> 11);
    const int32_t temp2 = (int32_t)(((((int64_t)delta2 * delta2) >> 12) * calibration.t3) >> 14);
    const int32_t t_fine = temp1 + temp2;

    // Cong thuc 64 bit, ket qua Q24.8 (Pa * 256).
    // Dung phep nhan thay cho dich trai so am de tranh undefined behavior.
    int64_t var1 = (int64_t)t_fine - 128000;
    int64_t var2 = var1 * var1 * calibration.p6;
    var2 += var1 * calibration.p5 * ((int64_t)1 << 17);
    var2 += (int64_t)calibration.p4 * ((int64_t)1 << 35);
    var1 = ((var1 * var1 * calibration.p3) >> 8) +
           var1 * calibration.p2 * ((int64_t)1 << 12);
    var1 = ((((int64_t)1 << 47) + var1) * calibration.p1) >> 33;
    if (var1 <= 0)
    {
        last_error = BMP280_ERROR_CALIBRATION;
        return NAN;
    }

    int64_t pressure = 1048576 - adc_pressure;
    pressure = (((pressure << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)calibration.p9 * (pressure >> 13) * (pressure >> 13)) >> 25;
    var2 = ((int64_t)calibration.p8 * pressure) >> 19;
    pressure = ((pressure + var1 + var2) >> 8) + (int64_t)calibration.p7 * 16;
    if (pressure <= 0 || pressure > UINT32_MAX)
    {
        last_error = BMP280_ERROR_INVALID_DATA;
        return NAN;
    }

    last_error = BMP280_ERROR_NONE;
    return (float)pressure / 256.0f;
}

static uint8_t bmp280_wait_measurement(void)
{
    for (uint16_t attempt = 0U; attempt < BMP280_STATUS_POLL_LIMIT; ++attempt)
    {
        uint8_t status, control;
        if (!bmp280_read_registers(BMP280_REG_STATUS, &status, 1U) ||
            !bmp280_read_registers(BMP280_REG_CTRL_MEAS, &control, 1U))
            return 0U;

        // Ve sleep + het measuring/NVM moi dam bao 6 byte khong thay doi.
        if ((status & (BMP280_STATUS_MEASURING | BMP280_STATUS_NVM)) == 0U &&
            (control & 0x03U) == 0U)
            return 1U;
        if (attempt + 1U < BMP280_STATUS_POLL_LIMIT)
            I2C_DelayMs(1U);
    }
    last_error = BMP280_ERROR_TIMEOUT;
    return 0U;
}

float readBmp280(void)
{
    if (!initialized)
    {
        last_error = BMP280_ERROR_NOT_INITIALIZED;
        return NAN;
    }
    if (!bmp280_write_register(BMP280_REG_CTRL_MEAS, BMP280_CTRL_FORCED))
        return NAN;

    // Nhiet do x1 + ap suat x4 can toi da 13.3 ms; cho 15 ms roi kiem tra.
    I2C_DelayMs(15U);
    if (!bmp280_wait_measurement())
        return NAN;

    uint8_t raw[6];
    if (!bmp280_read_registers(BMP280_REG_PRESSURE, raw, sizeof(raw)))
        return NAN;

    // Forced mode da ve sleep: 0xF7..0xFC giu nguyen trong suot luc doc.
    const int32_t adc_pressure = ((int32_t)raw[0] << 12) |
                                 ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    const int32_t adc_temperature = ((int32_t)raw[3] << 12) |
                                    ((int32_t)raw[4] << 4) | (raw[5] >> 4);
    if (adc_pressure == 0x80000 || adc_temperature == 0x80000)
    {
        last_error = BMP280_ERROR_INVALID_DATA;
        return NAN;
    }

    return bmp280_compensate_pressure(adc_pressure, adc_temperature);
}

uint8_t bmp280_get_last_error(void)
{
    return last_error;
}

uint8_t bmp280_get_address(void)
{
    return detected_address;
}

uint8_t bmp280_get_chip_id(void)
{
    return detected_chip_id;
}
