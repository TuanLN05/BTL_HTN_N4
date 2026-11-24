# BTL_HTN_N4
Bài tập lớn HTN, Thiết kế hệ thống giám sát môi trường thời gian thực sử dụng STM32, cập nhật firmware từ xa qua OTA

## Các thay đổi đã thực hiện

Phần firmware nằm trong thư mục `HTNN1_N4/`. Các thay đổi dưới đây tập trung vào giao diện OLED, nút bấm và chẩn đoán I2C; không triển khai thêm các task cảm biến do thành viên khác phụ trách.

### Chuẩn hóa comment

- Đổi comment khối `/* ... */` trong code ứng dụng thành `//`.
- Chỉnh nội dung comment thành giải thích chức năng và cách sử dụng code.
- Giữ nguyên thư viện FreeRTOS bên ngoài. Việc chuẩn hóa comment không thay đổi logic chương trình.

### Đổi driver OLED từ SH1107 sang SH1106

Màn thực tế là SH1106 128×64. Đã đổi tên thư mục, file, macro, kiểu dữ liệu, hàm và các chỗ sử dụng tương ứng:

- `lib/sh1107/sh1107.h` → `lib/sh1106/sh1106.h`.
- `src/sh1107/sh1107.c` → `src/sh1106/sh1106.c`.
- `SH1107_*` → `SH1106_*`; `Sh1107` → `Sh1106`.
- Cập nhật include trong giao diện, menu và cấu hình IntelliSense.

Trong `HTNN1_N4/src/sh1106/sh1106.c`, đã chỉnh chuỗi khởi tạo:

- Dùng `0xAD, 0x8B` để bật DC-DC của SH1106, thay cho `0xAD, 0x8A`.
- Đặt offset hàng bằng `0`, cấu hình multiplex 64 hàng và COM cho màn 128×64.
- Ghi dữ liệu theo page, bắt đầu từ cột RAM `2` để phù hợp module 128×64 thông dụng.
- Thêm khoảng chờ 100 ms trước lệnh `DISPLAY ON` (`0xAF`).

Hàm `SH1106_Init()` nhận thêm callback `Sh1106DelayMs`. Callback `Board_DelayMs()` dùng `vTaskDelay()` để chờ trong task mà không giữ CPU. `App_UI_Init()` đã được cập nhật để truyền callback này.

### Khôi phục I2C lúc khởi tạo

Trong `HTNN1_N4/src/i2c/i2c.c`, đã thêm `I2C_ResetBus()`:

- Tắt ngoại vi I2C, tạm chuyển PB6/PB7 sang GPIO open-drain.
- Tạo các chuyển mức SDA/SCL và kiểm tra mức chân để hỗ trợ khôi phục tình trạng kẹt bus/bộ lọc analog của STM32F1.
- Khôi phục alternate-function open-drain, thực hiện software reset và reset ngoại vi qua `RCC->APB1RSTR` trước khi cấu hình lại I2C.

Trình tự này chỉ chạy khi khởi tạo, trước khi các task sử dụng bus. Cấu hình hiện tại vẫn giả định HSI/APB1 8 MHz và I2C 100 kHz; chưa tự tính lại timing nếu thay đổi clock hệ thống.

### Gán lại ba nút bấm

`Board_ReadButton()` trong `HTNN1_N4/src/bsp/board_port.c` dùng bảng chân `{12, 14, 13}` theo thứ tự UP, DOWN, SELECT.

| Chức năng | Chân STM32 | Cách nối |
|---|---|---|
| UP | PB12 | Nút nối xuống GND khi nhấn |
| DOWN | PB14 | Nút nối xuống GND khi nhấn |
| SELECT | PB13 | Nút nối xuống GND khi nhấn |

Các chân dùng pull-up nội: chưa nhấn đọc `1`, nhấn đọc `0`. Cơ chế nút hiện có vẫn được giữ nguyên: debounce 25 ms, phát sự kiện khi **nhấn rồi thả**, không tự lặp khi giữ nút. Task giao diện quét định kỳ 20 ms.

Tại màn hình HOME, SELECT mở danh sách menu; UP/DOWN chỉ di chuyển khi đang ở trang có thao tác tương ứng.

### Thêm biến chẩn đoán

Các biến OLED được khai báo trong `HTNN1_N4/lib/app/task_display.h` và cập nhật trong task/adapter I2C:

| Biến | Ý nghĩa |
|---|---|
| `g_oled_stage` | `0`: chưa vào task; `1`: chờ nguồn; `2`: đang khởi tạo; `3`: khởi tạo lỗi; `4`: chạy giao diện |
| `g_oled_init_attempts` | Số lần thử khởi tạo giao diện |
| `g_oled_last_i2c_error` | Kết quả giao dịch ghi OLED gần nhất |
| `g_oled_render_result` | `0`: không cần vẽ; `1`: đã vẽ; `-1`: lỗi vẽ/I2C |
| `g_i2c_pin_samples[5]` | Các mẫu `GPIOB->IDR` trong trình tự khôi phục bus; mẫu chưa thực hiện giữ giá trị ban đầu |

Mã lỗi ghi I2C: `0` thành công, `1` không ACK, `2` lỗi bus, `3` timeout, `4` tham số sai, `5` không lấy được mutex, `6` mutex chưa sẵn sàng.

## Kết nối OLED hiện tại

| Tín hiệu OLED | Kết nối |
|---|---|
| SCL | PB6 |
| SDA | PB7 |
| GND | GND chung với STM32 |
| VCC | Nguồn phù hợp thông số module đang dùng |
| Địa chỉ I2C | `0x3C` dạng 7-bit, không dịch trái trước khi gọi driver |

## Kết quả kiểm tra và giới hạn

- Đã build và upload qua ST-Link thành công, có bước verify firmware.
- Ban đầu firmware trên board không khớp bản build hiện tại; đã nạp lại.
- Từng ghi nhận task OLED khởi tạo lỗi, I2C timeout và cờ BUSY bật. Ở các lần kiểm tra sau, khởi tạo thành công ngay lần đầu, `g_oled_stage = 4` và lỗi I2C bằng `0`.
- Đã gửi lệnh chẩn đoán qua GPIO: OLED ACK tại `0x3C`, sáng toàn bộ điểm ảnh, sau đó hiện menu khi trở về chế độ hiển thị RAM. Người dùng xác nhận menu vẫn hiện sau reset.
- Đã đo được chuyển mức của UP/PB12, DOWN/PB14 và SELECT/PB13. Việc đo tín hiệu chưa thay thế kiểm thử đầy đủ mọi thao tác menu.
- Chưa chốt chắc chắn nguyên nhân duy nhất của lỗi màn đen ban đầu; chưa có xác nhận riêng về lần bật nguồn hoàn toàn sau thay đổi khoảng chờ 100 ms.

Hai vấn đề logic đã ghi nhận nhưng **chưa sửa**:

- SAVE RTC hiện cập nhật giờ trên giao diện khi yêu cầu vào queue thành công; chưa xác nhận RTC thật đã ghi thành công.
- Adapter đổi ngưỡng bỏ qua kết quả gửi queue; nếu queue đầy, yêu cầu có thể mất dù menu đã lưu giá trị mới trong RAM.

## Build và upload

Chạy từ thư mục chứa README này:

```powershell
pio run -d HTNN1_N4
pio run -d HTNN1_N4 -t upload
```

Nếu terminal đang ở trong `HTNN1_N4/`, dùng `pio run` và `pio run -t upload`.
