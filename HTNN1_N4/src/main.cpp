#include "uartdma.h"
#include "aht20.h"
#include <stdio.h>

// Gui nhiet do (C) va do am (%) voi hai chu so thap phan.
bool uart_send_temp_humi(float temp, float humi)
{
    char buffer[128];
    int length = snprintf(buffer, sizeof(buffer),
                          "Temp: %.2f C, Humi: %.2f %%\r\n",
                          static_cast<double>(temp), static_cast<double>(humi));
    if (length <= 0 || static_cast<size_t>(length) >= sizeof(buffer))
    {
        return false;
    }

    return dma_uart_writebytes(reinterpret_cast<const uint8_t *>(buffer),
                               static_cast<uint16_t>(length));
}

int main(void)
{
    i2c1_init();
    aht20_init();
    dma_uart(115200);

    float temp = 0.0f;
    float humi = 0.0f;
    while (1)
    {
        readAHT20(&temp, &humi);
        uart_send_temp_humi(temp, humi);
    }
}
