#include "stm32f103xb.h"
#include "uartdma.h"
#include "i2c.h"
#include "bmp280.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void uart_write_text(const char *text)
{
    dma_uart_writebytes((const uint8_t *)text, (uint16_t)strlen(text));
}

int main(void)
{
    // Bat UART truoc de co the xem ca loi khoi tao cam bien.
    dma_uart(115200);
    uart_write_text("BMP280 starting - UART 115200, 8N1\r\n");
    uint8_t sensor_ready = 0U;

    while (1)
    {
        char buffer[128];
        if (!sensor_ready)
        {
            sensor_ready = bmp280_init();
            if (!sensor_ready)
            {
                snprintf(buffer, sizeof(buffer), "BMP280 init failed, error=%u\r\n",
                         (unsigned int)bmp280_get_last_error());
                uart_write_text(buffer);
                I2C_DelayMs(1000U);
                continue;
            }
            snprintf(buffer, sizeof(buffer), "BMP280 init OK, address=0x%02X chip_id=0x%02X\r\n",
                     (unsigned int)bmp280_get_address(), (unsigned int)bmp280_get_chip_id());
            uart_write_text(buffer);
        }

        float pressure = readBmp280();
        if (isnan(pressure))
        {
            snprintf(buffer, sizeof(buffer), "BMP280 read failed, error=%u\r\n",
                     (unsigned int)bmp280_get_last_error());
            sensor_ready = 0U;
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "Pressure: %.2f Pa\r\n", (double)pressure);
        }
        uart_write_text(buffer);
        I2C_DelayMs(1000U);
    }
}
