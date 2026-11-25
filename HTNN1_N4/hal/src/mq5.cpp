#include "mq5.h"
#include "hal_gpio.h"
#include <math.h>

namespace
{
constexpr uint32_t timeout_ms = 10U;
bool adc_ready;

bool wait_adc(volatile uint32_t &reg, uint32_t mask, bool set)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t timeout = (SystemCoreClock / 1000U) * timeout_ms;
    do
    {
        if (((reg & mask) != 0U) == set)
            return true;
    } while (DWT->CYCCNT - start < timeout);
    return false;
}
}

void MQ5_Init(void)
{
    adc_ready = false;
    gpio_Config(GPIOA, 0, GPIO_ANALOG);
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;

    SystemCoreClockUpdate();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    ADC1->CR2 = 0U;
    const uint32_t prescaler = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    const uint32_t pclk2 = SystemCoreClock >> (prescaler < 4U ? 0U : prescaler - 3U);
    const uint32_t adc_prescaler = pclk2 > 28000000U ? RCC_CFGR_ADCPRE_DIV6 : RCC_CFGR_ADCPRE_DIV2;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_ADCPRE) | adc_prescaler;

    ADC1->CR1 = 0U;
    ADC1->SQR1 = 0U;
    ADC1->SQR3 = 0U; // Mot kenh chuyen doi, PA0 (ADC1_IN0).
    ADC1->SMPR2 |= ADC_SMPR2_SMP0;
    ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL;

    const uint32_t start = DWT->CYCCNT;
    while (DWT->CYCCNT - start < SystemCoreClock / 100000U) {} // 10 us sau khi bat ADC.

    ADC1->CR2 |= ADC_CR2_RSTCAL;
    if (wait_adc(ADC1->CR2, ADC_CR2_RSTCAL, false))
    {
        ADC1->CR2 |= ADC_CR2_CAL;
        adc_ready = wait_adc(ADC1->CR2, ADC_CR2_CAL, false);
    }
    if (!adc_ready)
        ADC1->CR2 = 0U;
}

bool readMQ5Raw(uint16_t *value)
{
    if (!adc_ready || !value)
        return false;

    ADC1->SR &= ~ADC_SR_EOC;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    if (!wait_adc(ADC1->SR, ADC_SR_EOC, true))
    {
        ADC1->CR2 = 0U;
        adc_ready = false;
        return false;
    }
    *value = static_cast<uint16_t>(ADC1->DR);
    return true;
}

float readMQ5()
{
    uint16_t adcValue;
    if (!readMQ5Raw(&adcValue))
        return NAN;
    // Chuyen doi gia tri ADC sang gia tri nồng độ khí (ppm)
    float ppm = (float)adcValue * 5.5f / 4095.0f; // Giả sử điện áp tham chiếu là 5.5V và ADC có độ phân giải 12 bit
    return ppm;
}
