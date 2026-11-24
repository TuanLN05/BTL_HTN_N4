#include "aht20.h"
#include <math.h>

namespace
{
constexpr uint32_t timeout_ms = 25U;
constexpr uint32_t bus_errors = I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR;
constexpr uint32_t error_flags = bus_errors | I2C_SR1_AF | I2C_SR1_TIMEOUT;
constexpr uint32_t pending = I2C_CR1_START | I2C_CR1_STOP;
constexpr uint32_t scl = 1U << 6U;
constexpr uint32_t sda = 1U << 7U;
constexpr uint32_t pins = scl | sda;
constexpr uint32_t pin_modes = 0xFFU << 24U;
constexpr uint32_t scl_hz = 80000U;
constexpr uint8_t frame_size = 6U;
constexpr uint8_t busy = 1U << 7U;
constexpr uint8_t calibrated = 1U << 3U;

uint32_t cycles_per_ms;
AHT20_Error last_error = AHT20_ERROR_NONE;
bool initialized;

void init_timer()
{
    if (cycles_per_ms != 0U)
        return;
    SystemCoreClockUpdate();
    cycles_per_ms = SystemCoreClock / 1000U;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_cycles(uint32_t cycles)
{
    const uint32_t start = DWT->CYCCNT;
    while (DWT->CYCCNT - start < cycles) {}
}

bool wait_flag(volatile uint32_t &reg, uint32_t mask, bool set = true)
{
    const uint32_t start = DWT->CYCCNT;
    do
    {
        const uint32_t status = I2C1->SR1 & error_flags;
        if (status != 0U)
        {
            if (status & bus_errors)
                last_error = AHT20_ERROR_I2C_BUS;
            else
                last_error = (status & I2C_SR1_AF) ? AHT20_ERROR_NACK : AHT20_ERROR_I2C_TIMEOUT;
            return false;
        }
        if (((reg & mask) != 0U) == set)
            return true;
    } while (DWT->CYCCNT - start < timeout_ms * cycles_per_ms);

    last_error = AHT20_ERROR_I2C_TIMEOUT;
    return false;
}

void reset_i2c_peripheral()
{
    // Giu timing va dia chi cua I2C1 vi RTC va BMP280 dung chung bus.
    const uint32_t control = I2C1->CR1 & ~(pending | I2C_CR1_POS);
    const uint32_t cr2 = I2C1->CR2;
    const uint32_t oar1 = I2C1->OAR1;
    const uint32_t oar2 = I2C1->OAR2;
    const uint32_t ccr = I2C1->CCR;
    const uint32_t trise = I2C1->TRISE;
    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0U;
    I2C1->CR2 = cr2;
    I2C1->OAR1 = oar1 | (1U << 14U);
    I2C1->OAR2 = oar2;
    I2C1->CCR = ccr;
    I2C1->TRISE = trise;
    I2C1->CR1 = control | I2C_CR1_PE;
}

bool abort_transfer()
{
    const AHT20_Error failure = last_error;
    if ((I2C1->SR2 & I2C_SR2_MSL) && !(I2C1->CR1 & pending))
        I2C1->CR1 |= I2C_CR1_STOP;
    I2C1->SR1 &= ~error_flags;
    if (!wait_flag(I2C1->CR1, pending, false) || !wait_flag(I2C1->SR2, I2C_SR2_BUSY, false))
        reset_i2c_peripheral();
    I2C1->CR1 |= I2C_CR1_ACK;
    last_error = failure;
    return false;
}

bool set_bus_lines(uint32_t levels)
{
    GPIOB->BSRR = levels | ((pins & ~levels) << 16U);
    delay_cycles((cycles_per_ms + 199U) / 200U);
    return (GPIOB->IDR & pins) == levels;
}

bool unlock_busy()
{
    // ES096: khoi phuc bo loc I2C khi BUSY=1 du ca SDA va SCL dang cao.
    const uint32_t original_modes = GPIOB->CRL & pin_modes;
    I2C1->CR1 &= ~I2C_CR1_PE;
    GPIOB->BSRR = pins;
    GPIOB->CRL = (GPIOB->CRL & ~pin_modes) | (0x66U << 24U);
    const bool released = set_bus_lines(pins) && set_bus_lines(scl) &&
                          set_bus_lines(0U) && set_bus_lines(scl) && set_bus_lines(pins);
    GPIOB->BSRR = pins;
    GPIOB->CRL = (GPIOB->CRL & ~pin_modes) | original_modes;
    reset_i2c_peripheral();
    return released;
}

bool prepare_bus()
{
    init_timer();
    const uint32_t prescaler = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    const uint32_t pclk1 = SystemCoreClock >> (prescaler < 4U ? 0U : prescaler - 3U);
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    (void)RCC->APB2ENR;
    if (pclk1 < 2000000U || pclk1 > 36000000U || (AFIO->MAPR & AFIO_MAPR_I2C1_REMAP))
    {
        last_error = AHT20_ERROR_I2C_CONFIG;
        return false;
    }
    if (!(RCC->APB1ENR & RCC_APB1ENR_I2C1EN))
        i2c1_init();

    last_error = AHT20_ERROR_NONE;
    if (!wait_flag(I2C1->CR1, pending, false))
        return abort_transfer();

    // Driver goc cau hinh PB6/PB7. Cap nhat timing theo APB1 thuc te.
    I2C1->CR1 &= ~I2C_CR1_PE;
    I2C1->CR2 = (I2C1->CR2 & ~I2C_CR2_FREQ) | (pclk1 / 1000000U);
    I2C1->CCR = (pclk1 + 2U * scl_hz - 1U) / (2U * scl_hz);
    I2C1->TRISE = pclk1 / 1000000U + 1U;
    I2C1->OAR1 |= 1U << 14U;
    GPIOB->BSRR = pins;
    I2C1->CR1 |= I2C_CR1_PE;
    delay_cycles((cycles_per_ms + 199U) / 200U);

    if ((GPIOB->IDR & pins) != pins || ((I2C1->SR2 & I2C_SR2_BUSY) && !unlock_busy()))
    {
        last_error = AHT20_ERROR_I2C_TIMEOUT;
        return false;
    }
    return wait_flag(I2C1->SR2, I2C_SR2_BUSY, false);
}

bool begin_transfer(bool read)
{
    init_timer();
    if (!(RCC->APB1ENR & RCC_APB1ENR_I2C1EN))
        i2c1_init();

    last_error = AHT20_ERROR_NONE;
    if (!wait_flag(I2C1->CR1, pending, false) || !wait_flag(I2C1->SR2, I2C_SR2_BUSY, false))
        return abort_transfer();
    I2C1->CR1 = (I2C1->CR1 & ~I2C_CR1_POS) | I2C_CR1_ACK | I2C_CR1_START;
    if (!wait_flag(I2C1->SR1, I2C_SR1_SB))
        return abort_transfer();
    I2C1->DR = (aht20_addr << 1U) | (read ? 1U : 0U);
    if (!wait_flag(I2C1->SR1, I2C_SR1_ADDR))
        return abort_transfer();
    return true;
}

void clear_address()
{
    (void)I2C1->SR1;
    (void)I2C1->SR2;
}

bool finish_transfer()
{
    if (!wait_flag(I2C1->CR1, I2C_CR1_STOP, false) || !wait_flag(I2C1->SR2, I2C_SR2_BUSY, false))
        return abort_transfer();
    I2C1->CR1 |= I2C_CR1_ACK;
    delay_cycles((cycles_per_ms + 199U) / 200U); // Khoang nghi 5 us giua hai giao dich.
    return true;
}

bool write_command(const uint8_t *data, uint8_t size)
{
    if (!begin_transfer(false))
        return false;
    clear_address();
    for (uint8_t i = 0; i < size; ++i)
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

// AHT20 doc 1 byte trang thai, 3 byte hieu chuan hoac 6 byte do.
bool read_data(uint8_t *data, uint8_t size)
{
    if (!begin_transfer(true))
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

    clear_address();
    while (size > 3U)
    {
        if (!wait_flag(I2C1->SR1, I2C_SR1_RXNE))
            return abort_transfer();
        *data++ = static_cast<uint8_t>(I2C1->DR);
        --size;
    }

    // RM0008: doi BTF, tat ACK truoc khi doc 3 byte cuoi.
    if (!wait_flag(I2C1->SR1, I2C_SR1_BTF))
        return abort_transfer();
    I2C1->CR1 &= ~I2C_CR1_ACK;
    data[0] = static_cast<uint8_t>(I2C1->DR);

    if (!wait_flag(I2C1->SR1, I2C_SR1_BTF))
        return abort_transfer();
    const uint32_t irq = __get_PRIMASK();
    __disable_irq();
    I2C1->CR1 |= I2C_CR1_STOP;
    data[1] = static_cast<uint8_t>(I2C1->DR);
    data[2] = static_cast<uint8_t>(I2C1->DR);
    __set_PRIMASK(irq);
    return finish_transfer();
}

bool restore_calibration()
{
    // Trinh tu AHT20/21 cua Aosong: doc va ghi lai hai byte cua tung thanh ghi.
    const uint8_t registers[] = {0x1BU, 0x1CU, 0x1EU};
    for (const uint8_t reg : registers)
    {
        const uint8_t request[] = {reg, 0U, 0U};
        if (!write_command(request, sizeof(request)))
            return false;
        delay(5U);

        uint8_t response[3];
        if (!read_data(response, sizeof(response)))
            return false;
        delay(10U);

        const uint8_t restore[] = {static_cast<uint8_t>(reg | 0xB0U), response[1], response[2]};
        if (!write_command(restore, sizeof(restore)))
            return false;
    }
    delay(10U);
    return true;
}

bool measure(uint8_t (&data)[frame_size])
{
    const uint8_t command[] = {0xACU, 0x33U, 0x00U};
    if (!write_command(command, sizeof(command)))
        return false;

    delay(80U);
    for (uint8_t attempt = 0; attempt < 5U; ++attempt)
    {
        if (attempt > 0U)
            delay(10U);
        uint8_t status;
        if (!read_data(&status, 1U))
            return false;
        if (!(status & busy))
            return read_data(data, sizeof(data));
    }
    last_error = AHT20_ERROR_MEASUREMENT_TIMEOUT;
    return false;
}
}

void delay(uint32_t time)
{
    init_timer();
    while (time--)
        delay_cycles(cycles_per_ms);
}

void aht20_init()
{
    initialized = false;
    if (!prepare_bus())
        return;
    delay(100U);

    uint8_t status;
    if (!read_data(&status, 1U))
        return;
    if (!(status & calibrated))
    {
        const uint8_t command[] = {0xBEU, 0x08U, 0x00U};
        if (!write_command(command, sizeof(command)) && last_error != AHT20_ERROR_NACK)
            return;
        delay(10U);
        if (!read_data(&status, 1U))
            return;
        if (!(status & calibrated) && (!restore_calibration() || !read_data(&status, 1U)))
            return;
    }
    if (!(status & calibrated))
    {
        last_error = AHT20_ERROR_NOT_CALIBRATED;
        return;
    }
    initialized = true;
}

void aht20_reset()
{
    initialized = false;
    const uint8_t command = AHT20_SOFTRESET;
    if (prepare_bus() && write_command(&command, 1U))
        delay(20U);
}

void readAHT20(float *temp, float *humi)
{
    if (temp)
        *temp = NAN;
    if (humi)
        *humi = NAN;
    if (!initialized)
        aht20_init();
    if (!initialized)
        return;

    uint8_t data[frame_size];
    if (!measure(data))
    {
        initialized = false;
        return;
    }

    const uint32_t raw_h = (static_cast<uint32_t>(data[1]) << 12U) |
                           (static_cast<uint32_t>(data[2]) << 4U) | (data[3] >> 4U);
    const uint32_t raw_t = (static_cast<uint32_t>(data[3] & 0x0FU) << 16U) |
                           (static_cast<uint32_t>(data[4]) << 8U) | data[5];
    if (temp)
        *temp = raw_t * (200.0f / 1048576.0f) - 50.0f;
    if (humi)
        *humi = raw_h * (100.0f / 1048576.0f);
}

float readAHT20_temp()
{
    float temp;
    readAHT20(&temp, nullptr);
    return temp;
}

float readAHT20_humi()
{
    float humi;
    readAHT20(nullptr, &humi);
    return humi;
}

AHT20_Error aht20_get_last_error()
{
    return last_error;
}
