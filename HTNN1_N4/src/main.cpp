#include "uartdma.h"
#include "aht20.h"
#include "mq5.h"
#include "rtc.h"
#include <stdio.h>

// Gui nhiet do (C) va do am (%) voi hai chu so thap phan
bool uart_send_temp_humi()
{
    float temp, humi;
    readAHT20(&temp, &humi);
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
void uart_send_MQ5()
{
    float mq5 = readMQ5();
    char buffer[64];
    int length = snprintf(buffer, sizeof(buffer), "MQ5: %.2f\r\n",
                          static_cast<double>(mq5));
    if (length <= 0 || static_cast<size_t>(length) >= sizeof(buffer))
    {
        return;
    }

    dma_uart_writebytes(reinterpret_cast<const uint8_t *>(buffer),
                        static_cast<uint16_t>(length));
}
      DATE rtc_date = {};
TIME rtc_time = {};
int main(void)
{
    i2c1_init();
    aht20_init();
    MQ5_Init();
    dma_uart(115200);
rtc_init();
    while (1)
    {     
        readRTC(&rtc_date, &rtc_time);
      
        uart_send_temp_humi();
        uart_send_MQ5();
    }
}
