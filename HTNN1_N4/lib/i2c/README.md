## Quy ước chung

- `addr`: địa chỉ thiết bị **7 bit**, không dịch trái trước khi truyền vào hàm.
- `reg`: địa chỉ thanh ghi 8 bit của thiết bị.
- `data`: con trỏ tới vùng dữ liệu cần gửi hoặc nhận.
- `len`: số byte gửi hoặc nhận; bộ đệm phải đủ kích thước.
- Địa chỉ và giá trị trong các ví dụ chỉ để minh họa; thay theo thiết bị thực tế.
- Driver hiện dùng **PB6 = SCL**, **PB7 = SDA**, có sử dụng **TIM2** cho timeout. Cấu hình tốc độ I2C 100 kHz đang giả định clock APB1 là 8 MHz.

## 1. Dùng I2C thường

Include `i2c.h`, gọi `I2C_Init()` một lần sau khi cấu hình clock hệ thống, trước khi giao tiếp thiết bị.

### Các hàm cần gọi

| Hàm | Công dụng |
| --- | --- |
| `I2C_Init()` | Khởi tạo I2C1, GPIO và TIM2 |
| `I2C_Write(addr, data, len)` | Gửi một mảng byte |
| `I2C_Read(addr, &value)` | Đọc trực tiếp một byte |
| `I2C_ReadBytes(addr, data, len)` | Đọc trực tiếp nhiều byte |
| `I2C_WriteReg8(addr, reg, value)` | Gửi địa chỉ thanh ghi rồi giá trị một byte |
| `I2C_ReadReg8(addr, reg, &value)` | Đọc một byte từ thanh ghi |
| `I2C_ReadRegBytes(addr, reg, data, len)` | Gửi địa chỉ thanh ghi, repeated START, rồi đọc nhiều byte |

Các hàm giao tiếp trên trả về `void`. Kiểm tra biến `I2C_Error` ngay sau mỗi lần gọi.

## 2. Dùng I2C với FreeRTOS

Thứ tự gọi:

1. Cấu hình clock hệ thống.
2. Gọi `I2C_Init()` để khởi tạo phần cứng.
3. Gọi `I2C_RTOS_Init()` để tạo mutex, kiểm tra kết quả trả về.
4. Tạo task và gọi `vTaskStartScheduler()`.
5. Trong các task, dùng các hàm `I2C_RTOS_*` để đọc/ghi.

`I2C_RTOS_Init()` chỉ tạo mutex, không thay thế `I2C_Init()`.

### Các hàm cần gọi

| Hàm | Công dụng |
| --- | --- |
| `I2C_RTOS_Init()` | Tạo mutex, trả về `0` khi thành công, `6` khi thất bại |
| `I2C_RTOS_Write(addr, data, len)` | Gửi mảng byte có bảo vệ mutex |
| `I2C_RTOS_ReadBytes(addr, data, len)` | Đọc trực tiếp nhiều byte có bảo vệ mutex |
| `I2C_RTOS_ReadRegBytes(addr, reg, data, len)` | Đọc từ thanh ghi; mutex bảo vệ toàn bộ giao dịch gồm repeated START |

Các hàm RTOS trả về mã lỗi trực tiếp. Dùng giá trị trả về thay vì đọc biến chung `I2C_Error`.

## 3. Mã lỗi

| Mã | Ý nghĩa |
| --- | --- |
| `0` | Thành công |
| `1` | Thiết bị không ACK |
| `2` | Lỗi bus |
| `3` | Timeout giao tiếp I2C |
| `4` | Tham số không hợp lệ |
| `5` | Không lấy được mutex trong thời gian chờ 100 ms (RTOS) |
| `6` | Mutex chưa được khởi tạo hoặc tạo thất bại (RTOS) |

100 ms là thời gian chờ lấy mutex, không phải timeout cho toàn bộ giao dịch I2C.
