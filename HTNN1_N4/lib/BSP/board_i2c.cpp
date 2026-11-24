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
    GPIOB->BRR = 1 << 7;
    BusDelay();
    GPIOB->BRR = 1 << 6;
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

int Board_I2CInit(void)
{
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        Board_I2CRecoverBus();
        i2c1_init();
        // Cho ngoai vi cap nhat BUSY sau khi bat PE.
        BusDelay();
        if ((GPIOB->IDR & 0xC0U) == 0xC0U &&
            !(I2C1->SR2 & I2C_SR2_BUSY)) return 0;
    }
    return -1;
}

