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
