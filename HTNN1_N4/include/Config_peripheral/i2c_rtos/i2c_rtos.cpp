#include "i2c_rtos.h"
#include "i2c.h"          
#include "FreeRTOS.h"
#include "semphr.h"
#include <stddef.h>

static SemaphoreHandle_t s_i2c_mutex = NULL;

uint8_t I2C_RTOS_Init(void)
{
    if (s_i2c_mutex == NULL)
        s_i2c_mutex = xSemaphoreCreateMutex();
    return s_i2c_mutex != NULL ? 0 : 6;
}

static uint8_t take_bus(void)
{
    if (s_i2c_mutex == NULL) return 6;
    // Chua giu Mutex thi task khac co the dung I2C.
    // Sau khi lay duoc, phai tra lai dung 1 lan.
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
        return 5;
    return 0;
}

uint8_t I2C_RTOS_Write(uint8_t addr, const uint8_t *data, uint16_t len)
{
    uint8_t rc = take_bus();
    if (rc) return rc;
    I2C_Write(addr, data, len);
    rc = I2C_Error;      // Lay loi TRUOC khi mo khoa.
    xSemaphoreGive(s_i2c_mutex);
    return rc;
}

uint8_t I2C_RTOS_ReadBytes(uint8_t addr, uint8_t *data, uint16_t len)
{
    uint8_t rc = take_bus();
    if (rc) return rc;
    I2C_ReadBytes(addr, data, len);
    rc = I2C_Error;
    xSemaphoreGive(s_i2c_mutex);
    return rc;
}

uint8_t I2C_RTOS_ReadRegBytes(uint8_t addr, uint8_t reg,
                              uint8_t *data, uint16_t len)
{
    uint8_t rc = take_bus();
    if (rc) return rc;
    // Mutex bao ve toan bo giao dich, gom ca repeated START.
    I2C_ReadRegBytes(addr, reg, data, len);
    rc = I2C_Error;
    xSemaphoreGive(s_i2c_mutex);
    return rc;
}