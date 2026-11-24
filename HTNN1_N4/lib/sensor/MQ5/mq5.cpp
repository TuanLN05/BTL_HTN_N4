#include "mq5.h"
#include "ADC.h"
#include "GPIO.h"
void MQ5_Init(void)
{
    gpio_Config(GPIOA, 0, GPIO_ANALOG); // Cau hinh chan PA0 la dau vao ADC
    adc1_config(0); // Cau hinh ADC1 cho kenh 0 (PA0)
}

float readMQ5()
{
    // Doc gia tri ADC tu chan PA0
    uint16_t adcValue = adc1_read(); // Ham ADC_Read() tuong ung voi chan PA0
    // Chuyen doi gia tri ADC sang gia tri nồng độ khí (ppm)
    float ppm = (float)adcValue * 5.5f / 4095.0f; // Giả sử điện áp tham chiếu là 5.5V và ADC có độ phân giải 12 bit
    return ppm;
}