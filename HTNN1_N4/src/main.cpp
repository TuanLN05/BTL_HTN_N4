#include "uartdma.h"
#include "aht20.h"
#include "mq5.h"
#include "rtc.h"
#include "bmp280.h"
#include <math.h>
#include <stdio.h>

static void init_sensor_uart()
{
    constexpr uint32_t baud = 115200U;
    SystemCoreClockUpdate();
    const uint32_t prescaler = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    const uint32_t pclk2 = SystemCoreClock >> (prescaler < 4U ? 0U : prescaler - 3U);

    dma_uart(baud);
    USART1->CR1 &= ~USART_CR1_UE;
    USART1->CR2 &= ~(USART_CR2_STOP | USART_CR2_CLKEN);
    USART1->BRR = (pclk2 + baud / 2U) / baud;
    USART1->CR1 |= USART_CR1_UE;

    const char message[] = "STM32: USART1 115200 8N1\r\n";
    dma_uart_writebytes(reinterpret_cast<const uint8_t *>(message), sizeof(message) - 1U);
}

// Gui nhiet do (C) va do am (%) voi hai chu so thap phan
bool uart_send_temp_humi()
{
    float temp, humi;
    readAHT20(&temp, &humi);
    char buffer[128];
    const AHT20_Error error = aht20_get_last_error();
    int length;
    if (error == AHT20_ERROR_NONE)
        length = snprintf(buffer, sizeof(buffer), "Temp: %.2f C, Humi: %.2f %%\r\n",
                          static_cast<double>(temp), static_cast<double>(humi));
    else
        length = snprintf(buffer, sizeof(buffer), "AHT20: error=%u\r\n",
                          static_cast<unsigned>(error));
    if (length <= 0 || length >= static_cast<int>(sizeof(buffer)))
        return false;
    return dma_uart_writebytes(reinterpret_cast<const uint8_t *>(buffer),
                               static_cast<uint16_t>(length));
}
void uart_send_MQ5()
{
    const float mq5 = readMQ5();
    char buffer[64];
    const int length = isnan(mq5)
        ? snprintf(buffer, sizeof(buffer), "MQ5: ADC timeout\r\n")
        : snprintf(buffer, sizeof(buffer), "MQ5: %.2f\r\n", static_cast<double>(mq5));
    if (length > 0 && length < static_cast<int>(sizeof(buffer)))
        dma_uart_writebytes(reinterpret_cast<const uint8_t *>(buffer),
                            static_cast<uint16_t>(length));
}

void uart_send_rtc()
{
    DATE date = {};
    TIME time = {};
    readRTC(&date, &time);

    char buffer[64];
    const int length = snprintf(buffer, sizeof(buffer),
                                "RTC: %02u/%02u/%04u %02u:%02u:%02u, weekday=%u\r\n",
                                static_cast<unsigned>(date.day),
                                static_cast<unsigned>(date.month),
                                static_cast<unsigned>(date.year),
                                static_cast<unsigned>(time.hour),
                                static_cast<unsigned>(time.minute),
                                static_cast<unsigned>(time.second),
                                static_cast<unsigned>(date.weekday));
    dma_uart_writebytes(reinterpret_cast<const uint8_t *>(buffer),
                        static_cast<uint16_t>(length));
}

void uart_send_bmp280()
{
    static bool ready = false;
    char buffer[64];
    int length;

    if (!ready && !bmp280_init())
    {
        length = snprintf(buffer, sizeof(buffer), "BMP280 init failed, error=%u\r\n",
                          static_cast<unsigned>(bmp280_get_last_error()));
    }
    else
    {
        const float pressure = readBmp280();
        ready = !isnan(pressure);
        if (ready)
            length = snprintf(buffer, sizeof(buffer), "BMP280: %.2f Pa\r\n",
                              static_cast<double>(pressure));
        else
            length = snprintf(buffer, sizeof(buffer), "BMP280 read failed, error=%u\r\n",
                              static_cast<unsigned>(bmp280_get_last_error()));
    }

    if (length > 0 && length < static_cast<int>(sizeof(buffer)))
        dma_uart_writebytes(reinterpret_cast<const uint8_t *>(buffer),
                            static_cast<uint16_t>(length));
}

int main(void)
{
    init_sensor_uart();
    i2c1_init();
    aht20_init();
    uart_send_temp_humi();
    const char mq5_init_message[] = "MQ5: init\r\n";
    dma_uart_writebytes(reinterpret_cast<const uint8_t *>(mq5_init_message), sizeof(mq5_init_message) - 1U);
    MQ5_Init();
 //   rtc_init();
    while (1)
    {
        uart_send_temp_humi();
    //    uart_send_rtc();
        uart_send_MQ5();
        uart_send_bmp280();
    }
}
