#include "ADC.h"
uint8_t adc_Config(GPIO_TypeDef *port, uint8_t pin)
{
    uint32_t channel;
    if (port == GPIOA && pin <= 7)
    {
        RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
        channel = pin;
    }
    else if (port == GPIOB && pin <= 1)
    {
        RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
        channel = (uint32_t)pin + 8;
    }
    else return 0;

    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    ADC1->CR2 = 0;
    ADC1->CR1 = 0;
    port->CRL &= ~((uint32_t)0xF << (pin * 4)); // Chan analog

    ADC1->SQR1 = 0;
    ADC1->SQR3 = channel;
    ADC1->SMPR2 = (ADC1->SMPR2 & ~((uint32_t)7 << (channel * 3)))
                | ((uint32_t)7 << (channel * 3));  // 239.5 chu ky
    ADC1->CR2 = (1 << 20) | (7 << 17);

    ADC1->CR2 |= (1 << 0);
    for (volatile uint32_t i = 0; i < 1000; i++) {}

    ADC1->CR2 |= (1 << 3);
    while (ADC1->CR2 & (1 << 3)) {}
    ADC1->CR2 |= (1 << 2);
    while (ADC1->CR2 & (1 << 2)) {}
    return 1;
}

uint16_t adc_Read(void)
{
    ADC1->SR &= ~(1 << 1);      // Xoa co EOC
    ADC1->CR2 |= (1 << 22);     // SWSTART
    while (!(ADC1->SR & (1 << 1))) {}
    return (uint16_t)ADC1->DR;
}