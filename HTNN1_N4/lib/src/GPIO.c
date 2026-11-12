#include "GPIO.h"

void gpio_Config(GPIO_TypeDef *port, uint8_t pin, uint8_t mode)
{
    if (pin > 15) return;
    if (mode != GPIO_ANALOG && mode != GPIO_INPUT && mode != GPIO_INPUT_PULLUP &&
        mode != GPIO_OUTPUT_LOW && mode != GPIO_OUTPUT_HIGH) return;

    if (port == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (port == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    else if (port == GPIOC) RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    else return;

    // Dat muc ban dau/pull-up truoc khi chuyen che do chan.
    uint32_t mask = (uint32_t)1 << pin;
    if (mode == GPIO_INPUT_PULLUP || mode == GPIO_OUTPUT_HIGH) port->BSRR = mask;
    else if (mode == GPIO_OUTPUT_LOW) port->BSRR = mask << 16;

    volatile uint32_t *config = (pin < 8) ? &port->CRL : &port->CRH;
    uint32_t shift = (uint32_t)(pin % 8) * 4;
    *config = (*config & ~((uint32_t)0xF << shift)) | (((uint32_t)mode & 0xF) << shift);
}

uint8_t readInputPin(GPIO_TypeDef *port, uint8_t pin)
{
    if (pin > 15 || (port != GPIOA && port != GPIOB && port != GPIOC)) return 0;
    return (uint8_t)((port->IDR >> pin) & 1);
}

void setPin(GPIO_TypeDef *port, uint8_t pin)
{
    if (pin > 15 || (port != GPIOA && port != GPIOB && port != GPIOC)) return;
    port->BSRR = (uint32_t)1 << pin;
}

void resetPin(GPIO_TypeDef *port, uint8_t pin)
{
    if (pin > 15 || (port != GPIOA && port != GPIOB && port != GPIOC)) return;
    port->BSRR = (uint32_t)1 << (pin + 16);
}
