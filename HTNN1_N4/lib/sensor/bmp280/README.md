# BMP280

Driver dung API `i2c.h` trong `include/Config_peripheral`, khong dung FreeRTOS.
Khong phu thuoc driver trong `test`.

```cpp
#include "bmp280.h"
#include <math.h>

if (bmp280_init()) {
    float pressure_pa = readBmp280();
    if (!isnan(pressure_pa)) {
        float pressure_hpa = pressure_pa / 100.0f;
    }
}
```

- `bmp280_init()`: tu thu 0x76/0x77 toi da 3 vong, reset, doc calibration,
  cau hinh nhiet do x1 va ap suat x4. Tra 1 khi thanh cong.
- `bmp280_reset()`: tu tim dia chi neu can, reset, cho NVM. Can init lai truoc khi doc.
- `readBmp280()`: kich forced mode, cho ve sleep, doc du lieu va tinh ap suat Pa.
  Tra NAN khi loi; xem `bmp280_get_last_error()`.
- `bmp280_get_address()` / `bmp280_get_chip_id()`: dia chi va ID da tim duoc.

Driver I2C goc co nhanh nhan >=3 byte phat STOP qua som. Thu vien tranh nhanh do
bang cach doc tung thanh ghi 1 byte, cach nhau 1 ms de STOP ket thuc. Calibration
la hang so; du lieu do chi duoc doc sau khi da xac nhan forced mode ve sleep,
nen khong co phep do moi chen vao giua 6 byte. Khong dung normal mode hay cho
task khac thay doi cau hinh BMP280 dong thoi.

Ban I2C/UART trong `include` hien gia dinh clock 8 MHz. Giu HSI 8 MHz voi firmware
CMSIS hien tai; thu vien BMP280 khong tu doi clock he thong.

Build/nap bang moi truong goc (da duoc chon mac dinh):

```powershell
pio run -e genericSTM32F103C8
pio run -e genericSTM32F103C8 -t upload
pio device monitor -e genericSTM32F103C8 -b 115200
```

Noi PB6 -> SCL, PB7 -> SDA, GND chung. `error=1` van co nghia la khong nhan ACK:
neu ca hai dia chi deu khong phan hoi, can kiem tra day, nguon va pull-up.

Tham khao: [Bosch BMP280 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf),
muc 3.6.2 (forced mode), 3.9/3.10 (readout), 3.11 (compensation).
