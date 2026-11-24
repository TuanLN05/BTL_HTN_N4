#include "i2c.h"

void i2c1_init(void)
{
    RCC->APB1ENR |= 1 << 21;
    RCC->APB2ENR |= 1 << 3;

    GPIOB->CRL &= ~(0xFFU << (6 * 4));
    GPIOB->CRL |= 0xFFU << (6 * 4); // PB6 va PB7

    // APB1 = 16 MHz, standard mode = 100 kHz.
    I2C1->CR2 = 16;
    I2C1->CCR = 80;
    I2C1->TRISE |= 17;
    I2C1->CR1 |= 1 << 0;
}

void i2c1_Start(void)
{
    I2C1->CR1 |= 1 << 10; // ACK
    I2C1->CR1 |= 1 << 8;  // START
    while (!(I2C1->SR1 & (1 << 0)));
}

void i2c1_Stop(void)
{
    I2C1->CR1 |= 1 << 9;
}

void i2c1_writeAddr(uint8_t addr)
{
    I2C1->DR = addr;
    while (!(I2C1->SR1 & (1 << 1)));
    (void)I2C1->SR2; // Clear ADDR sau khi da doc SR1.
}

void i2c1_writeData(uint8_t data)
{
    while (!(I2C1->SR1 & (1 << 7)));
    I2C1->DR = data;
    while (!(I2C1->SR1 & (1 << 2)));
}

void i2c1_writeBytes(uint8_t addr, uint8_t *data, uint8_t size)
{
    i2c1_Start();
    i2c1_writeAddr(addr << 1);
    for (int i = 0; i < size; i++)
    {
        i2c1_writeData(data[i]);
    }
    i2c1_Stop();
}

void i2c1_readBytes(uint8_t addr, uint8_t *data, uint8_t size)
{
    i2c1_Start();
    i2c1_writeAddr((addr << 1) | 1);
    for (int i = 0; i < size; i++)
    {
        if (i == size - 1)
        {
            I2C1->CR1 &= ~(1 << 10); // NACK cho byte cuoi.
        }
        while (!(I2C1->SR1 & (1 << 6)));
        data[i] = I2C1->DR;
    }
    i2c1_Stop();
}
