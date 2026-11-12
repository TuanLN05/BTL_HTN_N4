
#include "i2c.h"

volatile uint8_t I2C_Error = 0;

// Khoi tao TIM2, moi tick bang 0.5ms
static void TIM2_Init(void)
{
    // Bat clock TIM2
    RCC->APB1ENR |= 0x00000001;

    // Tat TIM2 truoc khi cau hinh
    TIM2->CR1 = 0x0000;

    // TIM2 clock = 8MHz
    // 8MHz / 4000 = 2000Hz
    TIM2->PSC = 3999;

    // Bo dem 16-bit
    TIM2->ARR = 0xFFFF;

    // Cap nhat prescaler
    TIM2->EGR = 0x0001;

    // Xoa co update
    TIM2->SR = 0x0000;

    // Dua bo dem ve 0
    TIM2->CNT = 0;

    // Bat TIM2
    TIM2->CR1 = 0x0001;
}

// Tao delay bang TIM2
void I2C_DelayMs(uint16_t ms)
{
    while (ms--)
    {
        uint16_t start = (uint16_t)TIM2->CNT;

        // Hai tick TIM2 bang 1ms
        while ((uint16_t)(TIM2->CNT - start) < 2)
        {
        }
    }
}

// Cho co trang thai I2C trong SR1
static uint8_t I2C_Wait(uint32_t flag)
{
    uint16_t start = (uint16_t)TIM2->CNT;

    // 200 tick bang 100ms
    while ((uint16_t)(TIM2->CNT - start) < 200)
    {
        uint32_t sr1 = I2C1->SR1;

        // AF: Khong nhan ACK
        if (sr1 & I2C_SR1_AF)
        {
            I2C_Error = 1;
            return 0;
        }

        // BERR, ARLO, OVR: Loi bus
        if (sr1 & (I2C_SR1_BERR |
                   I2C_SR1_ARLO |
                   I2C_SR1_OVR))
        {
            I2C_Error = 2;
            return 0;
        }

        // Kiem tra co mong muon
        if (sr1 & flag)
            return 1;
    }

    // Timeout 100ms
    I2C_Error = 3;
    return 0;
}

// Cho bus I2C ranh
static uint8_t I2C_BusBusy(void)
{
    uint16_t start = (uint16_t)TIM2->CNT;

    while ((uint16_t)(TIM2->CNT - start) < 200)
    {
        // BUSY = 0: Bus ranh
        if (!(I2C1->SR2 & I2C_SR2_BUSY))
            return 1;
    }

    I2C_Error = 3;
    return 0;
}

// Tao START va gui dia chi Slave
static uint8_t I2C_Start(uint8_t addr, uint8_t direction)
{
    // Tao START
    I2C1->CR1 |= I2C_CR1_START;

    // Cho SB = 1
    if (!I2C_Wait(I2C_SR1_SB))
        return 0;

    // Gui dia chi 7-bit va bit R/W
    // direction = 0: Write
    // direction = 1: Read
    I2C1->DR = (addr << 1) | (direction & 1);

    // Cho Slave phan hoi ACK
    if (!I2C_Wait(I2C_SR1_ADDR))
        return 0;

    return 1;
}

// Xoa co ADDR
static void I2C_ClearADDR(void)
{
    // Doc SR1 roi SR2 de xoa ADDR
    (void)I2C1->SR1;
    (void)I2C1->SR2;
}

// Tao STOP
static void I2C_Stop(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;
}

// Xu ly loi giao tiep
static void I2C_Abort(void)
{
    // Chi phat STOP neu STM32 dang la Master
    if (I2C1->SR2 & I2C_SR2_MSL)
        I2C_Stop();

    // Xoa cac co loi
    I2C1->SR1 &= ~(I2C_SR1_AF |
                   I2C_SR1_BERR |
                   I2C_SR1_ARLO |
                   I2C_SR1_OVR);

    // Khoi phuc cau hinh nhan
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_ACK;
}

// Khoi tao I2C1
void I2C_Init(void)
{
    // Khoi tao timer cho timeout
    TIM2_Init();

    // Bat clock AFIO va GPIOB
    RCC->APB2ENR |= 0x00000009;

    // Bat clock I2C1
    RCC->APB1ENR |= 0x00200000;

    // Khong remap I2C1
    // PB6 = SCL, PB7 = SDA
    AFIO->MAPR &= ~0x00000002;

    // Dua chan PB6 va PB7 len muc cao
    GPIOB->BSRR = 0x000000C0;

    // Xoa cau hinh PB6 va PB7
    GPIOB->CRL &= 0x00FFFFFF;

    // PB6 SCL, PB7 SDA
    // PB6, PB7: Alternate Function Open Drain
    GPIOB->CRL |= 0xFF000000;

    // Tat I2C truoc khi cau hinh
    I2C1->CR1 &= ~0x0001;

    // Clock APB1 = 36MHz
    I2C1->CR2 = 0x08;

    // Standard mode 100kHz
    // CCR = 36 MHz / (2 * 100 kHz) = 180
    I2C1->CCR = 0x28;

    // TRISE = 37
    I2C1->TRISE = 0x09;

    // Bat bit 14 cua OAR1
    I2C1->OAR1 = 0x4000;

    // Bat I2C va ACK
    I2C1->CR1 = 0x0401;

    I2C_Error = 0;
}

// Gui nhieu byte den Slave
void I2C_Write(uint8_t addr,
               const uint8_t *data,
               uint16_t len)
{
    I2C_Error = 0;

    // Kiem tra tham so
    if (addr > 0x7F || (len > 0 && data == 0))
    {
        I2C_Error = 4;
        return;
    }

    // Cho bus ranh
    if (!I2C_BusBusy())
        return;

    // START + ADDRESS WRITE
    if (!I2C_Start(addr, 0))
    {
        I2C_Abort();
        return;
    }

    // Xoa ADDR
    I2C_ClearADDR();

    // Gui tung byte
    for (uint16_t i = 0; i < len; i++)
    {
        // Cho DR san sang
        if (!I2C_Wait(I2C_SR1_TXE))
        {
            I2C_Abort();
            return;
        }

        // Gui byte du lieu
        I2C1->DR = data[i];
    }

    // Cho byte cuoi truyen xong
    if (len > 0 && !I2C_Wait(I2C_SR1_BTF))
    {
        I2C_Abort();
        return;
    }

    // Ket thuc giao tiep
    I2C_Stop();
}

// Nhan nhieu byte khi ADDR da bang 1
static void I2C_ReceiveBytes(uint8_t *data, uint16_t len)
{
    uint32_t primask;

    // Truong hop nhan 1 byte
    if (len == 1)
    {
        // Luu trang thai ngat
        primask = __get_PRIMASK();
        __disable_irq();

        // Tat ACK de gui NACK sau byte duy nhat
        I2C1->CR1 &= ~I2C_CR1_ACK;

        // Xoa ADDR
        I2C_ClearADDR();

        // Yeu cau STOP
        I2C_Stop();

        // Khoi phuc trang thai ngat
        __set_PRIMASK(primask);

        // Cho nhan duoc du lieu
        if (!I2C_Wait(I2C_SR1_RXNE))
        {
            I2C_Abort();
            return;
        }

        data[0] = (uint8_t)I2C1->DR;
    }

    // Truong hop nhan 2 byte
    else if (len == 2)
    {
        // POS da duoc bat truoc khi gui dia chi
        primask = __get_PRIMASK();
        __disable_irq();

        // Xoa ADDR
        I2C_ClearADDR();

        // Tat ACK de NACK byte thu hai
        I2C1->CR1 &= ~I2C_CR1_ACK;

        __set_PRIMASK(primask);

        // Cho nhan du 2 byte
        if (!I2C_Wait(I2C_SR1_BTF))
        {
            I2C_Abort();
            return;
        }

        primask = __get_PRIMASK();
        __disable_irq();

        // Tao STOP va doc 2 byte
        I2C_Stop();

        data[0] = (uint8_t)I2C1->DR;
        data[1] = (uint8_t)I2C1->DR;

        __set_PRIMASK(primask);
    }

    // Truong hop nhan 3 byte tro len
    else
    {
        // Bat dau cho phep nhan du lieu
        I2C_ClearADDR();

        // Doc cac byte truoc 3 byte cuoi
        while (len > 3)
        {
            if (!I2C_Wait(I2C_SR1_RXNE))
            {
                I2C_Abort();
                return;
            }

            *data++ = (uint8_t)I2C1->DR;
            len--;
        }

        // Cho 2 byte dang nam trong DR va shift
        if (!I2C_Wait(I2C_SR1_BTF))
        {
            I2C_Abort();
            return;
        }

        primask = __get_PRIMASK();
        __disable_irq();

        // Tat ACK truoc byte cuoi
        I2C1->CR1 &= ~I2C_CR1_ACK;

        // Doc byte N-2
        *data++ = (uint8_t)I2C1->DR;

        // Yeu cau STOP cho byte cuoi
        I2C_Stop();

        // Doc byte N-1
        *data++ = (uint8_t)I2C1->DR;

        __set_PRIMASK(primask);

        // Cho byte N vao DR
        if (!I2C_Wait(I2C_SR1_RXNE))
        {
            I2C_Abort();
            return;
        }

        // Doc byte N
        *data = (uint8_t)I2C1->DR;
    }

    // Khoi phuc POS va ACK
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_ACK;
}

// Doc nhieu byte tu Slave
void I2C_ReadBytes(uint8_t addr,
                   uint8_t *data,
                   uint16_t len)
{
    I2C_Error = 0;

    if (addr > 0x7F || data == 0 || len == 0)
    {
        I2C_Error = 4;
        return;
    }

    // Cho bus ranh
    if (!I2C_BusBusy())
        return;

    // Cau hinh ACK va POS
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_ACK;

    // Nhan 2 byte can POS = 1
    if (len == 2)
        I2C1->CR1 |= I2C_CR1_POS;

    // START + ADDRESS READ
    if (!I2C_Start(addr, 1))
    {
        I2C_Abort();
        return;
    }

    // Nhan du lieu
    I2C_ReceiveBytes(data, len);
}

// Doc 1 byte tu Slave
void I2C_Read(uint8_t addr, uint8_t *data)
{
    I2C_ReadBytes(addr, data, 1);
}

// Ghi 1 byte vao thanh ghi cua Slave
void I2C_WriteReg8(uint8_t addr,
                   uint8_t reg,
                   uint8_t value)
{
    // Byte 0: Dia chi thanh ghi
    // Byte 1: Gia tri can ghi
    uint8_t buffer[2] = {reg, value};

    I2C_Write(addr, buffer, 2);
}

// Doc nhieu thanh ghi lien tiep
void I2C_ReadRegBytes(uint8_t addr,
                      uint8_t reg,
                      uint8_t *data,
                      uint16_t len)
{
    I2C_Error = 0;

    if (addr > 0x7F || data == 0 || len == 0)
    {
        I2C_Error = 4;
        return;
    }

    // Cho bus ranh
    if (!I2C_BusBusy())
        return;

    // START + ADDRESS WRITE
    if (!I2C_Start(addr, 0))
    {
        I2C_Abort();
        return;
    }

    // Xoa ADDR
    I2C_ClearADDR();

    // Cho DR san sang
    if (!I2C_Wait(I2C_SR1_TXE))
    {
        I2C_Abort();
        return;
    }

    // Gui dia chi thanh ghi can doc
    I2C1->DR = reg;

    // Cho gui xong dia chi thanh ghi
    if (!I2C_Wait(I2C_SR1_BTF))
    {
        I2C_Abort();
        return;
    }

    // Cau hinh nhan du lieu
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_ACK;

    if (len == 2)
        I2C1->CR1 |= I2C_CR1_POS;

    // Repeated START + ADDRESS READ
    // Khong doi BUSY ve 0 o day
    if (!I2C_Start(addr, 1))
    {
        I2C_Abort();
        return;
    }

    // Nhan nhieu byte
    I2C_ReceiveBytes(data, len);
}

// Doc 1 thanh ghi cua Slave
void I2C_ReadReg8(uint8_t addr,
                  uint8_t reg,
                  uint8_t *data)
{
    I2C_ReadRegBytes(addr, reg, data, 1);
}
