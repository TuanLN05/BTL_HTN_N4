PB6 SCL, PB7 SDA

Address 7 bit:
OLED SSD1306 0x3C
AHT20 0x38
BMP280 0x76 or 0x77
RTC DS1307 0x68
AT24C32 0x50

// 0: Thanh cong
// 1: Slave khong ACK
// 2: Loi bus
// 3: Timeout
// 4: Tham so khong hop le
extern volatile uint8_t I2C_Error;
debug bằng stlink để xem có thể có lỗi gì

I2C_Write(addr, data, len)	Ghi len byte den dia chi Slave 7-bit.
I2C_Read(addr, data)	Doc 1 byte tu Slave.
I2C_ReadBytes(addr, data, len)	Doc len byte lien tiep tu Slave.
I2C_WriteReg8(addr, reg, value)	Ghi 1 byte vao thanh ghi reg cua Slave.
I2C_ReadReg8(addr, reg, data)	Doc 1 byte tu thanh ghi reg.
I2C_ReadRegBytes(addr, reg, data, len)	Doc nhieu byte tu thanh ghi bat dau reg, su dung Repeated START.
