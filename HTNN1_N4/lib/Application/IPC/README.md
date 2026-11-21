# Gửi dữ liệu từ task Sensor sang task Display

Task Sensor đọc cảm biến bằng driver của mình, sau đó gọi API trong
`app_ipc.h` để gửi kết quả. Task Display nhận dữ liệu và cập nhật OLED.

## 1. Hàm Sensor cần gọi

```c
BaseType_t Display_PostSensor(MenuSensor id, int32_t value, uint8_t valid);
```

- `id`: loại dữ liệu theo bảng bên dưới.
- `value`: giá trị số nguyên theo đơn vị quy định.
- `valid`: `1` nếu đọc thành công, `0` nếu lỗi hoặc mẫu không hợp lệ.
- Kết quả: `pdPASS` khi gửi được, `pdFALSE` khi queue chưa tạo, ID sai
  hoặc queue đầy. Chỉ dùng các ID hợp lệ trong bảng.

| ID | Giá trị gửi | Ví dụ |
|---|---|---|
| `MENU_AHT_TEMP` | Nhiệt độ AHT, °C × 10 | 25,3°C → `253` |
| `MENU_AHT_HUM` | Độ ẩm AHT, %RH × 10 | 65,0% → `650` |
| `MENU_BMP_TEMP` | Nhiệt độ BMP, °C × 10 | −3,2°C → `-32` |
| `MENU_BMP_PRESS` | Áp suất BMP, hPa × 10 | 1013,2 hPa → `10132` |
| `MENU_MQ5_ADC` | Giá trị ADC thô 12 bit, 0–4095 | ADC 1800 → `1800` |

Nếu driver trả áp suất theo Pa, chia cho `10` để ra đơn vị hPa × 10.
Nếu driver đã trả giá trị nhân 10 thì không nhân thêm lần nữa.
MQ5 hiện hiển thị ADC thô, chưa nhận nồng độ ppm.

Ví dụ gửi một mẫu đã đọc thành công:

```c
if (Display_PostSensor(MENU_AHT_TEMP, 253, 1) != pdPASS) {
    // Ghi nhận lỗi gửi hoặc gửi mẫu mới nhất ở chu kỳ sau.
}
```

Khi đọc lỗi, báo mẫu không hợp lệ để OLED hiện `--`:

```c
(void)Display_PostSensor(MENU_AHT_TEMP, 0, 0);
```

Nếu AHT đọc lỗi cả nhiệt độ và độ ẩm, gửi `valid = 0` cho cả hai ID.
Giá trị `0` với `valid = 1` vẫn là một phép đo hợp lệ, không biểu thị lỗi.

## 2. Phân chia công việc giữa các task

- Sensor: đọc driver, kiểm tra lỗi, chuyển đơn vị và gọi `Display_PostSensor()`.
- Display: nhận queue, sửa `Menu`, xử lý nút và vẽ OLED.
- RTC: gọi `Display_PostRTC()` để gửi ngày giờ; đây là API dành cho task RTC.
- Điều khiển: nhận `ThresholdRequest` từ `g_limit_request_queue` nếu cần
  cập nhật ngưỡng do người dùng lưu. Queue này không tự phát lại ngưỡng
  ban đầu; phần khởi tạo task điều khiển cần có cách nạp ngưỡng riêng.

Task Sensor không gọi trực tiếp `Menu_SetSensor()`, `Menu_Render()` hoặc
các hàm vẽ SH1106. Với cảm biến dùng chung I2C1 với OLED, driver cần đi
qua `I2C_RTOS` để dùng cùng mutex. Các hàm IPC trên dùng trong task,
không gọi từ ISR.