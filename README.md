
# STM32F103C8T6 - I2C Driver

## 1. Chan ket noi I2C1

- PB6: SCL
- PB7: SDA

## 2. Dia chi I2C (7-bit)

| Module | Address |
|---|---|
| OLED SSD1306 | 0x3C |
| AHT20 | 0x38 |
| BMP280 | 0x76 hoac 0x77 |
| RTC DS1307 | 0x68 |
| EEPROM AT24C32 | 0x50 |

## 3. Kiem tra loi I2C

```c
// 0: Thanh cong
// 1: Slave khong ACK
// 2: Loi bus
// 3: Timeout
// 4: Tham so khong hop le
extern volatile uint8_t I2C_Error;
```

Su dung ST-Link Debug trong VS Code de theo doi bien `I2C_Error`.

- `0`: Khong phat hien loi giao tiep.
- `1`: Slave khong phan hoi ACK.
- `2`: Loi bus I2C.
- `3`: Qua thoi gian cho.
- `4`: Tham so truyen vao khong hop le.

Trong che do Debug, them `I2C_Error` vao WATCH de xem ma loi sau khi goi ham I2C.

## 4. Cac ham su dung

| Ham | Chuc nang |
|---|---|
| `I2C_Write(addr, data, len)` | Ghi len byte den dia chi Slave 7-bit. |
| `I2C_Read(addr, data)` | Doc 1 byte tu Slave. |
| `I2C_ReadBytes(addr, data, len)` | Doc len byte lien tiep tu Slave. |
| `I2C_WriteReg8(addr, reg, value)` | Ghi 1 byte vao thanh ghi reg cua Slave. |
| `I2C_ReadReg8(addr, reg, data)` | Doc 1 byte tu thanh ghi reg. |
| `I2C_ReadRegBytes(addr, reg, data, len)` | Doc nhieu byte tu thanh ghi bat dau reg, su dung Repeated START. |
