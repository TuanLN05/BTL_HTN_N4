#include "ADC.h"

namespace {
constexpr uint32_t dmaPollLimit = 10000U;
volatile uint16_t adcSample;
bool adcReady = false;
}

uint8_t adc1_config(uint8_t channel)
{
    if (channel > 9) return 0;

    adcReady = false;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->AHBENR;
    (void)RCC->APB2ENR;

    ADC1->CR2 = 0;
    ADC1->CR1 = 0;

    // ADC1 dung DMA1 Channel 1, chuyen 16 bit tu ADC1->DR sang RAM.
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF1;
    DMA1_Channel1->CPAR = reinterpret_cast<uintptr_t>(&ADC1->DR);
    DMA1_Channel1->CMAR = reinterpret_cast<uintptr_t>(&adcSample);
    DMA1_Channel1->CCR = DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0;

    ADC1->SQR1 = 0;                                 // Mot kenh chuyen doi
    ADC1->SQR3 = channel;                           // Kenh dau tien trong chuoi
    ADC1->SMPR2 |= (uint32_t)7 << (channel * 3);    // Lay mau 239.5 chu ky

    ADC1->CR2 = ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL | ADC_CR2_ADON;
    for (volatile uint32_t i = 0; i < 1000; i++) {}

    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while (ADC1->CR2 & ADC_CR2_RSTCAL) {}
    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL) {}

    ADC1->CR2 |= ADC_CR2_DMA;
    adcReady = true;
    return 1;
}

uint16_t adc1_read(void)
{
    if (!adcReady) return 0;

    // Nap lai bo dem DMA truoc moi lan kich mot phep do.
    DMA1->IFCR = DMA_IFCR_CGIF1;
    ADC1->SR &= ~ADC_SR_EOC;
    DMA1_Channel1->CNDTR = 1U;
    DMA1_Channel1->CCR |= DMA_CCR_EN;
    ADC1->CR2 |= ADC_CR2_SWSTART;

    uint32_t status = 0;
    for (uint32_t poll = 0; poll < dmaPollLimit; ++poll)
    {
        status = DMA1->ISR;
        if (status & (DMA_ISR_TCIF1 | DMA_ISR_TEIF1)) break;
    }

    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF1;
    if ((status & (DMA_ISR_TCIF1 | DMA_ISR_TEIF1)) != DMA_ISR_TCIF1)
    {
        ADC1->CR2 = 0;
        adcReady = false;
        return 0;
    }

    __DMB();
    return adcSample;
}