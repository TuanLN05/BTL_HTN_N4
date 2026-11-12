#include "ADC.h"

uint8_t adc1_config(uint8_t channel)
{
    if (channel > 9) return 0;

    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    ADC1->CR2 = 0;
    ADC1->CR1 = 0;

    ADC1->SQR1 = 0;                                 // Mot kenh chuyen doi
    ADC1->SQR3 = channel;                           // Kenh dau tien trong chuoi
    ADC1->SMPR2 |= (uint32_t)7 << (channel * 3);    // Lay mau 239.5 chu ky

    ADC1->CR2 = (1U << 20) | (7U << 17);            // EXTTRIG, EXTSEL = SWSTART

    ADC1->CR2 |= (1U << 0);                         // Bat ADC
    for (volatile uint32_t i = 0; i < 1000; i++) {}

    ADC1->CR2 |= (1U << 3);                         // Reset calibration
    while (ADC1->CR2 & (1U << 3)) {}
    ADC1->CR2 |= (1U << 2);                         // Calibration
    while (ADC1->CR2 & (1U << 2)) {}

    return 1;
}

uint16_t adc1_read(void)
{
    ADC1->SR &= ~(1U << 1);                         // Xoa co EOC
    ADC1->CR2 |= (1U << 22);                        // SWSTART
    while (!(ADC1->SR & (1U << 1))) {}
    return (uint16_t)ADC1->DR;
}