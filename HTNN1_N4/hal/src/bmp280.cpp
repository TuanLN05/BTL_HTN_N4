#include "bmp280.h"
#include "hal_i2c.h"
#include <math.h>

namespace
{
constexpr uint8_t reg_calibration = 0x88U;
constexpr uint8_t reg_id = 0xD0U;
constexpr uint8_t reg_reset = 0xE0U;
constexpr uint8_t reg_status = 0xF3U;
constexpr uint8_t reg_control = 0xF4U;
constexpr uint8_t reg_data = 0xF7U;
constexpr uint8_t expected_id = 0x58U;
constexpr uint8_t nvm_busy = 0x01U;
constexpr uint8_t measuring = 0x08U;
constexpr uint8_t forced = 0x2DU; // Nhiet do x1, ap suat x4, do mot lan.
static_assert(BMP280_I2C_ADDRESS == 0x76U || BMP280_I2C_ADDRESS == 0x77U,
              "BMP280 I2C address must be 0x76 or 0x77");

struct Calibration
{
    uint16_t t1;
    int16_t t2, t3;
    uint16_t p1;
    int16_t p2, p3, p4, p5, p6, p7, p8, p9;
};

Calibration cal;
uint8_t sensor_address;
uint8_t chip_id;
uint8_t last_error = BMP280_ERROR_NOT_INITIALIZED;
bool initialized;
void (*wait_ms_callback)(uint32_t);

constexpr uint32_t scl = 1U << 6U; // PB6
constexpr uint32_t sda = 1U << 7U; // PB7
constexpr uint32_t pins = scl | sda;
constexpr uint32_t pin_modes = 0xFFU << 24U;
constexpr uint32_t scl_hz = 80000U; // ES096: tranh repeated START o 88-100 kHz.
constexpr uint32_t timeout_ms = 25U;
constexpr uint32_t bus_errors = I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR;
constexpr uint32_t error_flags = bus_errors | I2C_SR1_AF | I2C_SR1_TIMEOUT;

uint32_t cycles_per_ms;
uint32_t pclk1_hz;

/**
 * @brief Cap nhat clock va bat bo dem chu ky DWT.
 */
void init_timer()
{
    SystemCoreClockUpdate();
    cycles_per_ms = SystemCoreClock / 1000U;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief Cho bang bo dem chu ky DWT.
 * @param[in] cycles So chu ky can cho.
 */
void delay_cycles(uint32_t cycles)
{
    const uint32_t start = DWT->CYCCNT;
    while (DWT->CYCCNT - start < cycles) {}
}

/**
 * @brief Cho trang thai bit I2C duoc yeu cau va ghi lai ma loi neu can.
 * @param[in] reg Thanh ghi I2C can theo doi.
 * @param[in] mask Mat na bit can kiem tra.
 * @param[in] set true de cho bit bat, false de cho tat het.
 * @retval true Da dat trang thai yeu cau.
 * @retval false Loi I2C hoac timeout.
 */
bool wait_flag(volatile uint32_t &reg, uint32_t mask, bool set = true)
{
    const uint32_t start = DWT->CYCCNT;
    do
    {
        const uint32_t status = I2C1->SR1;
        if (status & error_flags)
        {
            if (status & bus_errors)
                last_error = BMP280_ERROR_I2C_BUS;
            else
                last_error = (status & I2C_SR1_AF) ? BMP280_ERROR_I2C_NACK : BMP280_ERROR_I2C_TIMEOUT;
            return false;
        }
        if (((reg & mask) != 0U) == set)
            return true;
    } while (DWT->CYCCNT - start < timeout_ms * cycles_per_ms);

    last_error = BMP280_ERROR_I2C_TIMEOUT;
    return false;
}

/**
 * @brief Reset I2C1 va nap timing 80 kHz theo clock APB1.
 */
void configure_timing()
{
    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0U;
    const uint32_t frequency_mhz = pclk1_hz / 1000000U;
    I2C1->CR2 = frequency_mhz;
    I2C1->CCR = (pclk1_hz + 2U * scl_hz - 1U) / (2U * scl_hz);
    I2C1->TRISE = frequency_mhz + 1U;
    I2C1->OAR1 = 1U << 14U;
    I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;
}

/**
 * @brief Dat muc cac duong SCL va SDA va kiem tra muc doc lai.
 * @param[in] levels Mat na cac chan can nha len HIGH.
 * @retval true Ca hai chan dat muc yeu cau.
 * @retval false It nhat mot chan chua dat muc yeu cau.
 */
bool set_lines(uint32_t levels)
{
    GPIOB->BSRR = levels | ((pins & ~levels) << 16U);
    delay_cycles((cycles_per_ms + 199U) / 200U); // It nhat 5 us.
    return (GPIOB->IDR & pins) == levels;
}

/**
 * @brief Tao chuoi canh SCL va SDA va khoi tao lai timing khi I2C1 bi ket BUSY.
 * @retval true Da tao duoc chuoi muc yeu cau.
 * @retval false Khong tao duoc mot muc trong chuoi.
 */
bool unlock_busy()
{
    // ES096: tao canh SDA/SCL khi ca hai day dang cao nhung BUSY bi ket.
    const uint32_t original_modes = GPIOB->CRL & pin_modes;
    I2C1->CR1 &= ~I2C_CR1_PE;
    GPIOB->CRL = (GPIOB->CRL & ~pin_modes) | (0x66U << 24U);
    const bool released = set_lines(pins) && set_lines(scl) && set_lines(0U) &&
                          set_lines(scl) && set_lines(pins);
    GPIOB->BSRR = pins;
    GPIOB->CRL = (GPIOB->CRL & ~pin_modes) | original_modes;
    configure_timing();
    return released;
}

/**
 * @brief Chuan bi clock, timing va cac duong I2C1 cho BMP280.
 * @retval true Bus da ranh.
 * @retval false Cau hinh, muc duong bus hoac giao dich loi.
 */
bool prepare_bus()
{
    init_timer();
    const uint32_t apb1 = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    pclk1_hz = SystemCoreClock >> (apb1 < 4U ? 0U : apb1 - 3U);
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    (void)RCC->APB2ENR;
    if (pclk1_hz < 2000000U || pclk1_hz > 36000000U || (AFIO->MAPR & AFIO_MAPR_I2C1_REMAP))
    {
        last_error = BMP280_ERROR_I2C_CONFIG;
        return false;
    }

    // Dung driver goc de bat clock I2C1 va cau hinh PB6/PB7.
    if (RCC->APB1ENR & RCC_APB1ENR_I2C1EN)
        I2C1->CR1 &= ~I2C_CR1_PE;
    i2c1_init();
    GPIOB->BSRR = pins;
    configure_timing();
    delay_cycles((cycles_per_ms + 199U) / 200U);
    if ((GPIOB->IDR & pins) != pins || ((I2C1->SR2 & I2C_SR2_BUSY) && !unlock_busy()))
    {
        last_error = BMP280_ERROR_I2C_TIMEOUT;
        return false;
    }
    return wait_flag(I2C1->SR2, I2C_SR2_BUSY, false);
}

/**
 * @brief Huy giao dich loi va giu ma loi BMP280 ban dau.
 * @retval false Giao dich bi huy.
 */
bool abort_transfer()
{
    const uint8_t failure = last_error;
    constexpr uint32_t pending = I2C_CR1_START | I2C_CR1_STOP;
    if ((I2C1->SR2 & I2C_SR2_MSL) && !(I2C1->CR1 & pending))
        I2C1->CR1 |= I2C_CR1_STOP;
    I2C1->SR1 &= ~error_flags;
    if (!wait_flag(I2C1->CR1, pending, false))
        configure_timing();
    I2C1->CR1 |= I2C_CR1_ACK;
    last_error = failure;
    return false;
}

/**
 * @brief Cho STOP hoan tat va bat lai ACK.
 * @retval true Giao dich da ket thuc.
 * @retval false Co loi hoac het thoi gian cho.
 */
bool finish_transfer()
{
    if (!wait_flag(I2C1->CR1, I2C_CR1_STOP, false))
        return abort_transfer();
    delay_cycles((cycles_per_ms + 199U) / 200U);
    I2C1->CR1 |= I2C_CR1_ACK;
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
 * @brief Phat START va gui dia chi thiet bi theo chieu doc hoac ghi.
 * @param[in] address Dia chi I2C 7 bit.
 * @param[in] read true de doc, false de ghi.
 * @retval true Da nhan ACK dia chi.
 * @retval false Loi bus, NACK hoac timeout.
 */
bool start_address(uint8_t address, bool read)
{
    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_flag(I2C1->SR1, I2C_SR1_SB))
        return false;
    I2C1->DR = (address << 1U) | (read ? 1U : 0U);
    return wait_flag(I2C1->SR1, I2C_SR1_ADDR);
}

/**
 * @brief Nap mot byte va doi co truyen xong.
 * @param[in] value Byte can ghi.
 * @retval true Da gui byte.
 * @retval false Loi bus hoac timeout.
 */
bool send_byte(uint8_t value)
{
    if (!wait_flag(I2C1->SR1, I2C_SR1_TXE))
        return false;
    I2C1->DR = value;
    return wait_flag(I2C1->SR1, I2C_SR1_BTF);
}

/**
 * @brief Bat dau giao dich ghi va chon thanh ghi BMP280.
 * @param[in] address Dia chi I2C 7 bit.
 * @param[in] reg Dia chi thanh ghi.
 * @retval true Da chon thanh ghi.
 * @retval false Loi bus, NACK hoac timeout.
 */
bool select_register(uint8_t address, uint8_t reg)
{
    last_error = BMP280_ERROR_NONE;
    if (!wait_flag(I2C1->SR2, I2C_SR2_BUSY, false) || !start_address(address, false))
        return false;
    clear_address();
    return send_byte(reg);
}

/**
 * @brief Nhan cac byte BMP280 va phat STOP sau dia chi doc.
 * @param[out] data Bo dem nhan.
 * @param[in] size So byte can doc.
 * @retval true Da nhan cac byte.
 * @retval false Loi bus hoac timeout.
 */
bool receive(uint8_t *data, uint8_t size)
{
    if (size == 1U)
    {
        // EV6_3: tat ACK truoc khi xoa ADDR, phat STOP ngay sau do.
        const uint32_t irq = __get_PRIMASK();
        __disable_irq();
        I2C1->CR1 &= ~I2C_CR1_ACK;
        clear_address();
        I2C1->CR1 |= I2C_CR1_STOP;
        __set_PRIMASK(irq);
        if (!wait_flag(I2C1->SR1, I2C_SR1_RXNE))
            return false;
        *data = static_cast<uint8_t>(I2C1->DR);
        return true;
    }

    // BMP280 doc 1, 6 hoac 24 byte. Voi doc nhieu byte, xu ly 3 byte cuoi theo RM0008.
    clear_address();
    while (size > 3U)
    {
        if (!wait_flag(I2C1->SR1, I2C_SR1_RXNE))
            return false;
        *data++ = static_cast<uint8_t>(I2C1->DR);
        --size;
    }
    if (!wait_flag(I2C1->SR1, I2C_SR1_BTF))
        return false;
    I2C1->CR1 &= ~I2C_CR1_ACK;
    *data++ = static_cast<uint8_t>(I2C1->DR);

    if (!wait_flag(I2C1->SR1, I2C_SR1_BTF))
        return false;
    const uint32_t irq = __get_PRIMASK();
    __disable_irq();
    I2C1->CR1 |= I2C_CR1_STOP;
    data[0] = static_cast<uint8_t>(I2C1->DR);
    data[1] = static_cast<uint8_t>(I2C1->DR);
    __set_PRIMASK(irq);
    return true;
}

/**
 * @brief Doc lien tiep cac thanh ghi BMP280 bang repeated START.
 * @param[in] address Dia chi I2C 7 bit.
 * @param[in] reg Dia chi thanh ghi dau tien.
 * @param[out] data Bo dem nhan.
 * @param[in] size So byte can doc.
 * @retval true Da doc day byte.
 * @retval false Giao dich loi.
 */
bool read_registers(uint8_t address, uint8_t reg, uint8_t *data, uint8_t size)
{
    if (!select_register(address, reg) || !start_address(address, true) || !receive(data, size))
        return abort_transfer();
    return finish_transfer();
}

/**
 * @brief Ghi mot byte vao thanh ghi cua BMP280 dang duoc chon.
 * @param[in] reg Dia chi thanh ghi.
 * @param[in] value Byte can ghi.
 * @retval true Da ghi thanh cong.
 * @retval false Giao dich loi.
 */
bool write_register(uint8_t reg, uint8_t value)
{
    if (!select_register(sensor_address, reg) || !send_byte(value))
        return abort_transfer();
    I2C1->CR1 |= I2C_CR1_STOP;
    return finish_transfer();
}

/**
 * @brief Tim BMP280 dung ID tai 0x76 hoac 0x77.
 * @retval true Tim thay va da luu dia chi.
 * @retval false Khong tim thay chip phu hop hoac loi I2C.
 */
bool check_chip()
{
    sensor_address = chip_id = 0U;
    if (!prepare_bus())
        return false;

    I2C_DelayMs(10U);
    const uint8_t candidates[] = {
        BMP280_I2C_ADDRESS,
        BMP280_I2C_ADDRESS == 0x76U ? 0x77U : 0x76U
    };
    bool wrong_chip = false;
    for (const uint8_t candidate : candidates)
    {
        uint8_t id;
        if (!read_registers(candidate, reg_id, &id, 1U))
        {
            if (last_error == BMP280_ERROR_I2C_NACK)
                continue;
            return false;
        }

        chip_id = id;
        if (chip_id == expected_id)
        {
            sensor_address = candidate;
            return true;
        }
        wrong_chip = true;
    }
    if (wrong_chip)
        last_error = BMP280_ERROR_CHIP_ID;
    return false;
}

/**
 * @brief Cho cac bit trang thai BMP280 duoc xoa.
 * @param[in] mask Mat na bit trang thai can cho.
 * @retval true Tat ca cac bit da tat.
 * @retval false Doc trang thai loi hoac timeout.
 */
bool wait_ready(uint8_t mask)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        uint8_t status;
        if (!read_registers(sensor_address, reg_status, &status, 1U))
            return false;
        if (!(status & mask))
            return true;
        I2C_DelayMs(1U);
    }
    last_error = BMP280_ERROR_TIMEOUT;
    return false;
}

/**
 * @brief Doc mot so nguyen 16 bit khong dau luu theo little-endian.
 * @param[in] data Con tro toi hai byte du lieu.
 * @return Gia tri da giai ma.
 */
uint16_t u16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0] | (data[1] << 8U));
}

/**
 * @brief Doc mot so nguyen 16 bit co dau luu theo little-endian.
 * @param[in] data Con tro toi hai byte du lieu.
 * @return Gia tri da giai ma.
 */
int16_t s16(const uint8_t *data)
{
    return static_cast<int16_t>(u16(data));
}

/**
 * @brief Ghep gia tri ADC 20 bit tu ba byte BMP280.
 * @param[in] data Con tro toi ba byte du lieu.
 * @return Gia tri ADC tho.
 */
int32_t adc20(const uint8_t *data)
{
    return (static_cast<int32_t>(data[0]) << 12U) | (data[1] << 4U) | (data[2] >> 4U);
}

/**
 * @brief Bu nhiet do va ap suat BMP280 bang he so hieu chuan.
 * @param[in] raw_pressure Gia tri ADC ap suat.
 * @param[in] raw_temp Gia tri ADC nhiet do.
 * @param[out] temperature_c Nhiet do theo do C neu tinh thanh cong. Co the la NULL.
 * @return Ap suat da bu theo Pa, hoac NAN neu du lieu hoac he so khong hop le.
 */
float compensate_pressure(int32_t raw_pressure, int32_t raw_temp, float *temperature_c)
{
    const int64_t t1 = (raw_temp >> 3) - 2 * static_cast<int32_t>(cal.t1);
    const int64_t t2 = (raw_temp >> 4) - static_cast<int32_t>(cal.t1);
    const int64_t t_fine = ((t1 * cal.t2) >> 11) + ((((t2 * t2) >> 12) * cal.t3) >> 14);
    const int64_t t = t_fine - 128000;
    const int64_t offset = t * t * cal.p6 + t * cal.p5 * (1LL << 17) + cal.p4 * (1LL << 35);
    const int64_t sensitivity = ((t * t * cal.p3) >> 8) + t * cal.p2 * (1LL << 12);
    const int64_t divisor = ((((1LL << 47) + sensitivity) * cal.p1) >> 33);
    if (divisor <= 0)
    {
        last_error = BMP280_ERROR_CALIBRATION;
        return NAN;
    }

    const int64_t pressure = ((((1048576LL - raw_pressure) << 31) - offset) * 3125) / divisor;
    const int64_t q24_8 = ((pressure + ((cal.p9 * (pressure >> 13) * (pressure >> 13)) >> 25)
                           + ((cal.p8 * pressure) >> 19)) >> 8) + cal.p7 * 16;
    if (q24_8 <= 0 || q24_8 > UINT32_MAX)
    {
        last_error = BMP280_ERROR_INVALID_DATA;
        return NAN;
    }
    if (temperature_c)
        *temperature_c = static_cast<float>(t_fine) / 5120.0f;
    return static_cast<float>(q24_8) / 256.0f;
}
}

void bmp280_set_delay(void (*wait_ms)(uint32_t))
{
    wait_ms_callback = wait_ms;
}

void I2C_DelayMs(uint16_t ms)
{
    if (wait_ms_callback) {
        wait_ms_callback(ms);
        return;
    }
    if (cycles_per_ms == 0U)
        init_timer();
    while (ms--)
        delay_cycles(cycles_per_ms);
}

uint8_t bmp280_reset(void)
{
    initialized = false;
    if (sensor_address == 0U && !check_chip())
        return 0U;
    if (!write_register(reg_reset, 0xB6U))
        return 0U;
    I2C_DelayMs(3U);
    return wait_ready(nvm_busy);
}

uint8_t bmp280_init(void)
{
    initialized = false;
    if (!check_chip() || !bmp280_reset())
        return 0U;

    uint8_t raw[24];
    if (!read_registers(sensor_address, reg_calibration, raw, sizeof(raw)))
        return 0U;
    // T1 va P1 khong dau. Cac he so con lai co dau, tat ca la little-endian.
    cal = {u16(raw), s16(raw + 2), s16(raw + 4),
           u16(raw + 6), s16(raw + 8), s16(raw + 10),
           s16(raw + 12), s16(raw + 14), s16(raw + 16),
           s16(raw + 18), s16(raw + 20), s16(raw + 22)};
    if (cal.p1 == 0U || (cal.t1 == 0xFFFFU && cal.p1 == 0xFFFFU))
    {
        last_error = BMP280_ERROR_CALIBRATION;
        return 0U;
    }
    initialized = true;
    return 1U;
}

float readBmp280Sample(float *temperature_c)
{
    if (temperature_c)
        *temperature_c = NAN;
    if (!initialized)
    {
        last_error = BMP280_ERROR_NOT_INITIALIZED;
        return NAN;
    }

    if (!write_register(reg_control, forced))
        return NAN;
    I2C_DelayMs(15U); // Bosch: phep do x1/x4 toi da 13.3 ms.
    if (!wait_ready(measuring | nvm_busy))
        return NAN;

    uint8_t raw[6];
    if (!read_registers(sensor_address, reg_data, raw, sizeof(raw)))
        return NAN;
    const int32_t pressure = adc20(raw);
    const int32_t temperature = adc20(raw + 3);
    if (pressure == 0x80000 || temperature == 0x80000)
    {
        last_error = BMP280_ERROR_INVALID_DATA;
        return NAN;
    }
    return compensate_pressure(pressure, temperature, temperature_c);
}

float readBmp280(void) { return readBmp280Sample(nullptr); }

uint8_t bmp280_get_last_error(void) { return last_error; }
uint8_t bmp280_get_address(void) { return sensor_address; }
uint8_t bmp280_get_chip_id(void) { return chip_id; }
