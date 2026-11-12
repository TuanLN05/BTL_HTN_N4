#include "GPIO.h"

static uint8_t gpio_Valid(GPIO_TypeDef *port, uint8_t pin)
{
    return (pin <= 15) && (port == GPIOA || port == GPIOB || port == GPIOC);
}

uint8_t gpio_Config(GPIO_TypeDef *port, uint8_t pin, uint8_t mode)
{
    if (!gpio_Valid(port, pin)) return 0;
    if (mode != GPIO_ANALOG && mode != GPIO_INPUT && mode != GPIO_INPUT_PULLUP &&
        mode != GPIO_OUTPUT_LOW && mode != GPIO_OUTPUT_HIGH &&
        mode != GPIO_AF_PP && mode != GPIO_AF_OD) return 0;

    if (port == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (port == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    else RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    uint32_t mask = (uint32_t)1 << pin;
    if (mode == GPIO_INPUT_PULLUP || mode == GPIO_OUTPUT_HIGH) port->BSRR = mask;
    else if (mode == GPIO_OUTPUT_LOW) port->BSRR = mask << 16;

    volatile uint32_t *config = (pin < 8) ? &port->CRL : &port->CRH;
    uint32_t shift = (uint32_t)(pin % 8) * 4;
    *config = (*config & ~((uint32_t)0xF << shift)) | (((uint32_t)mode & 0xF) << shift);

    return 1;
}

uint8_t gpio_Read(GPIO_TypeDef *port, uint8_t pin)
{
    if (!gpio_Valid(port, pin)) return 0;
    return (uint8_t)((port->IDR >> pin) & 1);
}

void gpio_Set(GPIO_TypeDef *port, uint8_t pin)
{
    if (!gpio_Valid(port, pin)) return;
    port->BSRR = (uint32_t)1 << pin;
}

void gpio_Reset(GPIO_TypeDef *port, uint8_t pin)
{
    if (!gpio_Valid(port, pin)) return;
    port->BSRR = (uint32_t)1 << (pin + 16);
}