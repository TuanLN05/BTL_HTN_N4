# BTL hệ thống nhúng — Nhóm 4

Repository gồm hai dự án PlatformIO:

- [HTNN1_N4](HTNN1_N4/README.md): firmware STM32F103C8 sử dụng CMSIS và FreeRTOS.
- [STM32_FOTA_usingESP32_writeRAM](STM32_FOTA_usingESP32_writeRAM/README.md): dự án ESP32 phục vụ cập nhật firmware STM32.

## Tổ chức firmware STM32

Lấy code từ nhánh làm việc của nhóm và bổ sung code theo cấu trúc sau trong `HTNN1_N4/`:

```text
bsp/          # Hỗ trợ board, ánh xạ chân và khởi tạo phần cứng
hal/inc/      # Header driver ngoại vi, cảm biến và OLED
hal/src/      # Mã nguồn driver tương ứng
middlewares/  # FreeRTOS Kernel, Port và Heap
app/inc/      # Header ứng dụng, task, IPC và FreeRTOSConfig.h
app/src/      # Mã nguồn ứng dụng, task và IPC
main.cpp      # Khởi tạo board, tạo task, chạy scheduler
```

Ba task FreeRTOS được khởi tạo trong `HTNN1_N4/main.cpp`. Hai queue dữ liệu nối TaskSensor → TaskAlarm → TaskUI được quản lý trong `app/src/app_ipc.c`.

Ví dụ driver MQ5 nằm tại `hal/inc/mq5.h` và `hal/src/mq5.cpp`; logic menu nằm tại `app/inc/menu.h` và `app/src/menu.cpp`.

Chi tiết cây thư mục, kết nối phần cứng, luồng task và lệnh build: [HTNN1_N4/README.md](HTNN1_N4/README.md).
