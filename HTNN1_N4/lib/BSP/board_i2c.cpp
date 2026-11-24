#include "board_i2c.h"
#include "i2c.h"

static void BusDelay(void)
{
    // TIM2 da duoc khoi tao dem 1 ms truoc khi phuc hoi bus.
    uint16_t start = (uint16_t)TIM2->CNT;
    while ((uint16_t)((uint16_t)TIM2->CNT - start) < 2) {}
}

// Phuc hoi bus truoc khi khoi tao driver I2C.
void Board_I2CRecoverBus(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    I2C1->CR1 &= ~I2C_CR1_PE;

    // PB6/PB7 output open-drain; muc 1 nha duong bus.
    GPIOB->BSRR = (1 << 6) | (1 << 7);
    GPIOB->CRL = (GPIOB->CRL & 0x00FFFFFF) | 0x77000000;
    BusDelay();
    for (uint8_t i = 0; i < 9; ++i) {
        GPIOB->BRR = 1 << 6;
        BusDelay();
        GPIOB->BSRR = 1 << 6;
        BusDelay();
    }
    // Tao STOP: SDA len trong khi SCL dang o muc cao.
    GPIOB->BRR = (1 << 6) | (1 << 7);
    BusDelay();
    GPIOB->BSRR = 1 << 6;
    BusDelay();
    GPIOB->BSRR = 1 << 7;
    BusDelay();

    GPIOB->CRL = (GPIOB->CRL & 0x00FFFFFF) | 0xFF000000;
    I2C1->CR1 = I2C_CR1_SWRST;
    BusDelay();
    I2C1->CR1 = 0;
}

