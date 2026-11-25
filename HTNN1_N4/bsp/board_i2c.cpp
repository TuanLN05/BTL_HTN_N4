#include "board_i2c.h"
#include "hal_i2c.h"

/**
 * @brief Cho hai tick TIM2 trong qua trinh phuc hoi bus.
 */
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
    const uint32_t prescaler = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    const uint32_t pclk = SystemCoreClock >> (prescaler < 4U ? 0U : prescaler - 3U);
    if (pclk < 2000000U || pclk > 36000000U)
        return -1;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        // Driver cu co hai lan ghi GPIO. Phuc hoi SAU driver de xoa BUSY
        // phat sinh khi PB6/PB7 di qua trang thai analog trung gian.
        i2c1_init();
        Board_I2CRecoverBus();
        // Recover reset I2C; nap lai timing, khong goi driver doi GPIO lan nua.
        I2C1->CR2 = pclk / 1000000U;
        I2C1->CCR = (pclk + 159999U) / 160000U; // SCL 80 kHz.
        I2C1->TRISE = pclk / 1000000U + 1U;
        I2C1->OAR1 = 1U << 14U;
        I2C1->CR1 = I2C_CR1_PE;
        // Cho ngoai vi cap nhat BUSY sau khi bat PE.
        BusDelay();
        if ((GPIOB->IDR & 0xC0) == 0xC0 &&
            !(I2C1->SR2 & I2C_SR2_BUSY)) return 0;
    }
    return -1;
}

namespace
{
constexpr uint32_t flags = I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO |
                           I2C_SR1_OVR | I2C_SR1_TIMEOUT;
constexpr uint32_t pending = I2C_CR1_START | I2C_CR1_STOP;
constexpr uint16_t timeout_ms = 25U;

/**
 * @brief Cho mot trong cac bit I2C dat muc yeu cau, co kiem tra timeout.
 * @param[in] reg Thanh ghi I2C can theo doi.
 * @param[in] mask Mat na bit can kiem tra.
 * @param[in] set true de cho bit bat, false de cho tat het.
 * @retval true Da dat trang thai yeu cau.
 * @retval false Co loi I2C hoac timeout.
 */
bool wait_flag(volatile uint32_t &reg, uint32_t mask, bool set = true)
{
    const uint16_t start = static_cast<uint16_t>(TIM2->CNT);
    do
    {
        if (I2C1->SR1 & flags)
            return false;
        if (((reg & mask) != 0U) == set)
            return true;
    } while (static_cast<uint16_t>(static_cast<uint16_t>(TIM2->CNT) - start) < timeout_ms);
    return false;
}

/**
 * @brief Ket thuc giao dich loi, xoa co loi va khoi phuc bus neu can.
 * @retval false Giao dich bi huy.
 */
bool abort_transfer()
{
    if ((I2C1->SR2 & I2C_SR2_MSL) && !(I2C1->CR1 & pending))
        I2C1->CR1 |= I2C_CR1_STOP;
    I2C1->SR1 &= ~flags;
    if (!wait_flag(I2C1->CR1, pending, false))
        (void)Board_I2CInit();
    I2C1->CR1 = (I2C1->CR1 & ~I2C_CR1_POS) | I2C_CR1_ACK;
    return false;
}

/**
 * @brief Cho bus ranh, gui START va dia chi I2C.
 * @param[in] address Dia chi I2C 7 bit.
 * @param[in] read true de doc, false de ghi.
 * @retval true Da nhan ACK dia chi.
 * @retval false Loi bus, NACK hoac timeout.
 */
bool begin_transfer(uint8_t address, bool read)
{
    if (!wait_flag(I2C1->CR1, pending, false) || !wait_flag(I2C1->SR2, I2C_SR2_BUSY, false))
        return abort_transfer();
    I2C1->CR1 = (I2C1->CR1 & ~I2C_CR1_POS) | I2C_CR1_ACK | I2C_CR1_START;
    if (!wait_flag(I2C1->SR1, I2C_SR1_SB))
        return abort_transfer();
    I2C1->DR = (address << 1U) | (read ? 1U : 0U);
    if (!wait_flag(I2C1->SR1, I2C_SR1_ADDR))
        return abort_transfer();
    return true;
}

/**
 * @brief Xoa co ADDR bang viec doc lan luot SR1 va SR2.
 */
void clear_address()
{
    (void)I2C1->SR1;
    (void)I2C1->SR2;
}

/**
 * @brief Cho STOP hoan tat va khoi phuc co ACK mac dinh.
 * @retval true Bus da ranh.
 * @retval false Giao dich khong the ket thuc binh thuong.
 */
bool finish_transfer()
{
    if (!wait_flag(I2C1->CR1, I2C_CR1_STOP, false) || !wait_flag(I2C1->SR2, I2C_SR2_BUSY, false))
        return abort_transfer();
    I2C1->CR1 = (I2C1->CR1 & ~I2C_CR1_POS) | I2C_CR1_ACK;
    return true;
}

}

bool Board_I2CWrite(uint8_t address, const uint8_t *data, uint16_t size)
{
    if (!data || !size || address > 0x7FU || !begin_transfer(address, false))
        return false;
    clear_address();
    for (uint16_t i = 0; i < size; ++i)
    {
        if (!wait_flag(I2C1->SR1, I2C_SR1_TXE))
            return abort_transfer();
        I2C1->DR = data[i];
    }
    if (!wait_flag(I2C1->SR1, I2C_SR1_BTF))
        return abort_transfer();
    I2C1->CR1 |= I2C_CR1_STOP;
    return finish_transfer();
}

bool Board_I2CRead(uint8_t address, uint8_t *data, uint8_t size)
{
    if (!data || !size || address > 0x7FU || !begin_transfer(address, true))
        return false;

    if (size == 1U)
    {
        // Nhan mot byte: tat ACK truoc khi xoa ADDR.
        const uint32_t irq = __get_PRIMASK();
        __disable_irq();
        I2C1->CR1 &= ~I2C_CR1_ACK;
        clear_address();
        I2C1->CR1 |= I2C_CR1_STOP;
        __set_PRIMASK(irq);
        if (!wait_flag(I2C1->SR1, I2C_SR1_RXNE))
            return abort_transfer();
        data[0] = static_cast<uint8_t>(I2C1->DR);
        return finish_transfer();
    }

    if (size == 2U)
    {
        I2C1->CR1 |= I2C_CR1_POS;
        const uint32_t irq = __get_PRIMASK();
        __disable_irq();
        clear_address();
        I2C1->CR1 &= ~I2C_CR1_ACK;
        __set_PRIMASK(irq);
    }
    else
    {
        clear_address();
        while (size > 3U)
        {
            if (!wait_flag(I2C1->SR1, I2C_SR1_RXNE))
                return abort_transfer();
            *data++ = static_cast<uint8_t>(I2C1->DR);
            --size;
        }

        // RM0008. Sau khi BTF bat, tat ACK truoc khi doc byte thu ba tu cuoi.
        if (!wait_flag(I2C1->SR1, I2C_SR1_BTF))
            return abort_transfer();
        I2C1->CR1 &= ~I2C_CR1_ACK;
        *data++ = static_cast<uint8_t>(I2C1->DR);
    }

    if (!wait_flag(I2C1->SR1, I2C_SR1_BTF))
        return abort_transfer();
    const uint32_t irq = __get_PRIMASK();
    __disable_irq();
    I2C1->CR1 |= I2C_CR1_STOP;
    data[0] = static_cast<uint8_t>(I2C1->DR);
    data[1] = static_cast<uint8_t>(I2C1->DR);
    __set_PRIMASK(irq);
    return finish_transfer();
}
