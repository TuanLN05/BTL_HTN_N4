#include "stm32f1xx.h"
#include "ADC.h"

// Gioi han so lan tham do thanh ghi, khong phai thoi gian ms.
#define ADC_TIMEOUT 100000U

static uint8_t adc_ready = 0U;

static void ADC_StartupDelay(void)
{
    for (volatile uint32_t i = 0U; i < 1000U; ++i)
    {
        __NOP();
    }
}

static ADC_Status ADC_WaitForCalibration(uint32_t mask)
{
    uint32_t timeout = ADC_TIMEOUT;
    while ((ADC1->CR2 & mask) != 0U)
    {
        if (--timeout == 0U)
        {
            ADC1->CR2 &= ~ADC_CR2_ADON;
            return ADC_STATUS_TIMEOUT;
        }
    }
    return ADC_STATUS_OK;
}

ADC_Status ADC_Init_Custom(void)
{
    adc_ready = 0U;

    // MQ5 A0 -> PA0 -> ADC1_IN0 theo so do de bai.
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    (void)RCC->APB2ENR;
    GPIOA->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);

    // PCLK2 / 6: 1.33 MHz khi PCLK2 = 8 MHz; 12 MHz khi PCLK2 = 72 MHz.
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_ADCPRE) | RCC_CFGR_ADCPRE_DIV6;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;

    ADC1->CR1 = 0U;
    ADC1->CR2 = 0U;
    ADC1->SR = 0U;

    // Mot kenh, can phai, single conversion, sampling time 239.5 cycles.
    ADC1->SQR1 = 0U;
    ADC1->SQR2 = 0U;
    ADC1->SQR3 = 0U;
    ADC1->SMPR2 = (ADC1->SMPR2 & ~ADC_SMPR2_SMP0) | ADC_SMPR2_SMP0;
    ADC1->CR2 = ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG;

    // Cho du thoi gian power-off truoc calibration va on dinh sau power-on.
    ADC_StartupDelay();
    ADC1->CR2 |= ADC_CR2_ADON;
    ADC_StartupDelay();

    ADC1->CR2 |= ADC_CR2_RSTCAL;
    if (ADC_WaitForCalibration(ADC_CR2_RSTCAL) != ADC_STATUS_OK)
    {
        return ADC_STATUS_TIMEOUT;
    }

    ADC1->CR2 |= ADC_CR2_CAL;
    if (ADC_WaitForCalibration(ADC_CR2_CAL) != ADC_STATUS_OK)
    {
        return ADC_STATUS_TIMEOUT;
    }

    adc_ready = 1U;
    return ADC_STATUS_OK;
}

uint16_t ADC_Read_MQ5(void)
{
    uint32_t timeout = ADC_TIMEOUT;
    if (adc_ready == 0U)
    {
        return ADC_READ_ERROR;
    }

    ADC1->SR &= ~ADC_SR_EOC;
    ADC1->CR2 |= ADC_CR2_SWSTART;

    while ((ADC1->SR & ADC_SR_EOC) == 0U)
    {
        if (--timeout == 0U)
        {
            // Khoi tao lai de tranh doc ket qua cu sau timeout.
            adc_ready = 0U;
            ADC1->CR2 &= ~ADC_CR2_ADON;
            return ADC_READ_ERROR;
        }
    }

    return (uint16_t)(ADC1->DR & ADC_MAX_VALUE);
}

uint32_t ADC_To_mV(uint16_t adc_value)
{
    if (adc_value > ADC_MAX_VALUE)
    {
        return ADC_MV_ERROR;
    }
    return ((uint32_t)adc_value * ADC_VREF_MV) / ADC_MAX_VALUE;
}
