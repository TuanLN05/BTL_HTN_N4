# Firmware STM32F103C8 — HTNN1_N4

Dự án sử dụng CMSIS và FreeRTOS. Các task viết bằng C và gọi BSP, HAL, menu qua giao diện dùng chung giữa C và C++.

## Cấu trúc thư mục

```text
HTNN1_N4/
├── bsp/                         # Hỗ trợ bo mạch và ánh xạ chân
│   ├── board.h / board.cpp      # Clock, NVIC, UART, TIM2 và GPIO nút nhấn
│   ├── board_pins.h             # Chân nút nhấn và mô tả kết nối ngoại vi
│   └── board_i2c.h / .cpp       # Khởi tạo, phục hồi và giao dịch I2C1
├── hal/
│   ├── inc/                    # Header driver
│   │   ├── hal_gpio.h
│   │   ├── hal_adc.h
│   │   ├── hal_i2c.h
│   │   ├── hal_uart.h
│   │   ├── aht20.h
│   │   ├── bmp280.h
│   │   ├── mq5.h
│   │   ├── rtc.h
│   │   └── sh1106.h
│   └── src/                    # Các file .cpp tương ứng với inc/
├── middlewares/
│   └── FreeRTOS-Kernel/        # Kernel V11.2.0, port ARM_CM3, heap_4
├── app/
│   ├── inc/
│   │   ├── FreeRTOSConfig.h    # Cấu hình FreeRTOS của ứng dụng
│   │   ├── app.h               # API khởi tạo IPC và log UART
│   │   ├── app_data.h          # Dữ liệu cảm biến và cảnh báo
│   │   ├── app_ipc.h           # API hai queue, mutex I2C và ngưỡng dùng chung
│   │   ├── app_log.h           # Log UART của ứng dụng
│   │   ├── task_sensor.h       # task_sensor_create()
│   │   ├── task_alarm.h        # task_alarm_create()
│   │   ├── task_ui.h           # task_ui_create()
│   │   ├── button.h            # Chống dội và tạo sự kiện nút
│   │   ├── menu.h              # Logic giao diện và cảnh báo
│   │   └── threshold_store.h   # Lưu/đọc ngưỡng ứng dụng trong Flash
│   └── src/                    # task_sensor.c, task_alarm.c, task_ui.c và các module ứng dụng
├── main.cpp                    # Khởi tạo board, tạo task, chạy scheduler
├── platformio.ini
├── .gitignore
└── README.md
```

## Luồng khởi động và task

`main.cpp` gọi `Board_Init()` và `App_Init()` để chuẩn bị phần cứng, IPC và log UART. Sau đó, bản đầy đủ gọi ba hàm `task_sensor_create()`, `task_alarm_create()` và `task_ui_create()`. Scheduler chỉ chạy khi các bước khởi tạo và tạo task thành công.

| Task | Priority | Stack (word 32 bit) | Hoạt động |
|---|---:|---:|---|
| `TaskSensor` | 2 | 640 | Đọc MQ5 theo chu kỳ 125 ms. Đọc AHT20, BMP280 và RTC theo chu kỳ 1 giây. |
| `TaskAlarm` | 3 | 192 | Chờ mẫu qua queue, kiểm tra ngưỡng và điều khiển LED PA1. |
| `TaskUI` | 1 | 640 | Đọc nút theo chu kỳ 10 ms với chống dội 25 ms. Làm mới OLED theo chu kỳ 400 ms hoặc khi có thao tác hay trạng thái cảnh báo đổi. |

`TaskSensor → TaskAlarm` dùng queue 4 phần tử. `TaskAlarm → TaskUI` dùng queue 1 phần tử với [xQueueOverwrite()](https://sources.freertos.org/media/2018/FreeRTOS_Reference_Manual_V10.0.0.pdf) để giữ mẫu mới nhất. TaskAlarm bật PA1 khi mẫu hợp lệ chạm hoặc vượt ít nhất một ngưỡng. Khi lưu ngưỡng thành công, TaskAlarm áp dụng giá trị mới ở mẫu kế tiếp.

TaskSensor và TaskUI dùng mutex I2C1. TaskSensor nhả mutex trong lúc AHT20 hoặc BMP280 chờ chuyển đổi. TaskUI gửi OLED từng đoạn 32 byte và đọc nút giữa các đoạn. Khởi tạo lại thiết bị hoặc I2C timeout có thể kéo dài một chu kỳ.

## Kết nối phần cứng

| Chức năng | Chân / ngoại vi |
|---|---|
| UP / DOWN / SELECT | PB12 / PB14 / PB13, pull-up, nhấn nối GND |
| LED cảnh báo | PA1, HIGH khi có cảnh báo, LOW khi hết cảnh báo |
| I2C1 SCL / SDA | PB6 / PB7, OLED + AHT20 + BMP280 + RTC |
| USART1 TX / RX | PA9 / PA10, 115200 baud, 8N1 |
| UART DMA TX / RX | DMA1 Channel 4 / Channel 5 |
| MQ5 | PA0, ADC1_IN0 |
| ADC DMA driver | DMA1 Channel 1 |
| Bộ đếm I2C BSP | TIM2, bước đếm 1 ms |
| FreeRTOS tick | SysTick, 1 ms |

Chân nút nhấn được khai báo trong `bsp/board_pins.h`. Các chân ngoại vi còn lại dùng ánh xạ cố định trong driver STM32F103. Hai page Flash cuối được dành để lưu ngưỡng: `0x0800F800` và `0x0800FC00`; dung lượng firmware giới hạn 63.488 byte.

## Build

Chạy trong thư mục `HTNN1_N4`:

```powershell
pio run -e genericSTM32F103C8
```

Firmware được tạo tại `.pio/build/genericSTM32F103C8/firmware.bin`.

`platformio.ini` đặt `src_dir = .` và dùng `build_src_filter` để chỉ biên dịch `main.cpp`, `bsp/`, `hal/src/` và `app/src/`. FreeRTOS được liên kết từ bản có sẵn trong `middlewares/FreeRTOS-Kernel`, sử dụng `library.json` để chọn kernel, port và heap; không biên dịch lặp lại qua bộ lọc mã nguồn. Xem tài liệu PlatformIO về [src_dir](https://docs.platformio.org/en/latest/projectconf/sections/platformio/options/directory/src_dir.html) và [build_src_filter](https://docs.platformio.org/en/latest/projectconf/sections/env/options/build/build_src_filter.html).

## Test riêng từng task trên kit

| Environment | Phần cứng cần dùng | Kết quả kiểm tra |
|---|---|---|
| `mock_sensor` | Cảm biến, RTC và UART 115200 | TaskSensor ghi rồi đọc lại queue. UART in `SENSOR QUEUE`, mặt nạ mẫu hợp lệ và ADC MQ5, cùng số đo chậm và RTC. |
| `mock_alarm` | LED ở PA1 | Dữ liệu giả lần lượt dưới ngưỡng, vượt ngưỡng và không hợp lệ. LED lặp LOW → HIGH → LOW, mỗi trạng thái 2 giây. |
| `mock_ui` | OLED và ba nút nhấn | Dữ liệu giả có thời gian thay đổi và cảnh báo MQ5. Thử chuyển trang, đóng cảnh báo và chỉnh ngưỡng trong RAM. |

Build và nạp từng bản bằng CMSIS-DAP từ thư mục `HTNN1_N4`:

```powershell
pio run -e mock_sensor -t upload
pio run -e mock_alarm -t upload
pio run -e mock_ui -t upload
```

Sau khi kiểm tra, nạp lại bản đầy đủ bằng `pio run -e genericSTM32F103C8 -t upload`.

## Thêm hoặc sửa code

- Thêm driver: khai báo trong `hal/inc/`, triển khai trong `hal/src/`.
- Thêm cấu hình riêng cho board: đặt trong `bsp/`.
- Thêm task bằng cặp `app/inc/task_xxx.h` và `app/src/task_xxx.c`. Khai báo `task_xxx_create()` trong header và gọi hàm từ `main.cpp`.
- Quản lý queue tại `app_ipc.h` và `app_ipc.c`.
- Chỉnh cấu hình RTOS: sửa `app/inc/FreeRTOSConfig.h`.
- Giữ mã nguồn thư viện FreeRTOS trong `middlewares/`; thêm module kernel từ cùng phiên bản và cập nhật `library.json` khi cần dùng API mới.

## Đối chiếu với cấu trúc cũ

| Vị trí cũ | Vị trí mới |
|---|---|
| `include/Config_peripheral/` | `hal/inc/hal_*.h`, `hal/src/hal_*.cpp` |
| `include/FreeRTOS-Kernel/` | `middlewares/FreeRTOS-Kernel/` |
| `include/FreeRTOS-Kernel/FreeRTOSConfig.h` | `app/inc/FreeRTOSConfig.h` |
| `lib/sensor/` | `hal/inc/`, `hal/src/` |
| `lib/Application/board_i2c.*` | `bsp/board_i2c.*` |
| `lib/Application/sh1106.*` | `hal/inc/sh1106.h`, `hal/src/sh1106.cpp` |
| `lib/Application/button.*`, `menu.*`, `threshold_store.*` | `app/inc/`, `app/src/` |
| `src/main.cpp` | `main.cpp`, `bsp/board.cpp` và các module khởi tạo, task, IPC, log trong `app/` |
