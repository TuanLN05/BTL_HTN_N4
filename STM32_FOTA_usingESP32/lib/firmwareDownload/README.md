# Tải firmware STM32 bằng ESP32

Thư viện dành cho project Arduino ESP32 hiện tại. Dùng WiFi, LittleFS, HTTP client,
cJSON và mbedTLS có sẵn trong framework, không cần thêm `lib_deps`.

## Chạy thử

1. Sửa `WIFI_SSID`, `WIFI_PASSWORD`, `MANIFEST_URL` trong `src/main.cpp`.
   URL dùng IP LAN của máy tính, ví dụ `http://192.168.1.10:8000/manifest.json`.
2. Tại thư mục gốc `BTL_HTN_N4`, chạy `python -m http.server 8000`.
   Cho ESP32 và máy tính vào cùng mạng, cho phép kết nối đến cổng 8000.
3. Build/upload project ESP32 và mở Serial Monitor ở 115200 baud.
4. Gửi ký tự `D` để tải. Thành công sẽ in bo, phiên bản, kích thước
   và đường dẫn `/stm32_firmware.bin` trên **flash của ESP32**.

5. Khi nguồn ổn định, nối GPIO17 → PA10, GPIO16 ← PA9, GPIO25 → BOOT0,
   GPIO26 → transistor đảo → NRST (kéo lên 3,3 V), và GND chung. Giữ BOOT1/PB2
   ở mức thấp. Nếu BOOT0 có jumper nối thẳng GND hoặc 3,3 V, tháo jumper để GPIO25 điều khiển.
6. Gửi `F`. ESP32 kiểm tra lại SHA-256 của file, vào bootloader ROM qua USART1
   (115200, 8E1), kiểm tra PID 0x0410 và các lệnh hỗ trợ, rồi xóa những trang
   1 KiB chứa ứng dụng từ 0x08000000. Sau khi ghi và so sánh từng byte đọc lại,
   ESP32 hạ BOOT0 và reset STM32. Nếu gặp lỗi ghi, giữ nguồn ổn định rồi gửi lại `F`.

Nút BOOT trên ESP32 vẫn dùng để thử bắt tay UART. `F` tự thực hiện bước này.
Sau một lượt `D` thành công với bản chương trình này, SHA-256 được lưu vào NVS
để `F` cũng dùng được sau khi ESP32 khởi động lại hoặc ngắt Wi-Fi.

### Khi kết nối máy chủ bị timeout

Serial in tên Wi-Fi, IP ESP32, gateway, subnet và URL manifest trước khi tải.
Kiểm tra URL trong log để xác nhận bo đang chạy đúng cấu hình vừa upload.
Nếu laptop và ESP32 cùng kết nối hotspot điện thoại, dùng IP Wi-Fi của laptop
trong URL. IP adapter Mobile Hotspot của laptop được dùng khi ESP32 kết nối vào
hotspot do laptop phát.

Trên Windows, kiểm tra profile của adapter Wi-Fi (Private/Public) và profile của
quy tắc cho phép Python trong Firewall. Python được cho phép trên Public vẫn có
thể bị chặn khi Wi-Fi hiện tại dùng Private. Chỉ mở TCP cổng 8000 cho đúng chương
trình Python và subnet đang dùng. Thử mở URL từ thiết bị khác trên cùng mạng;
HTTP 200 khi mở từ chính laptop chưa xác nhận đường vào từ ESP32.

## API

```cpp
#include <firmwareDownload.h>
#include <LittleFS.h>

FirmwareDownloader downloader;
FirmwareInfo info;

// Thực hiện trong setup(), loop() hoặc một task; không đặt ở phạm vi toàn cục.
// begin(true) cho phép format khi mount thất bại; begin() mặc định không format.
if (downloader.begin(true) &&
    downloader.connectWiFi("TEN_WIFI", "MAT_KHAU") &&
    downloader.download("http://192.168.1.10:8000/manifest.json", info)) {
    File image = LittleFS.open(downloader.filePath(), FILE_READ);
    // Đọc image theo từng khối cho bước nạp STM32, sau đó image.close().
} else {
    Serial.println(downloader.lastError());
}
```

Các hàm là đồng bộ, có thời gian chờ kết nối Wi-Fi (mặc định 15 giây), thời gian
chờ mạng 10 giây và giới hạn mỗi lượt tải 60 giây được kiểm tra giữa các lần đọc.
Một lần đọc đang chờ mạng có thể kéo dài thêm tối đa thời gian chờ mạng.
Chỉ gọi từ một task tại một thời điểm. Hàm `download()` gán `info` khi thành công.
Thư viện tải phiên bản chỉ định trong manifest; ứng dụng quyết định khi nào cần
cập nhật và so sánh với phiên bản STM32 đang chạy.

## Manifest

```json
{
  "board": "STM32F103C8",
  "version": "1.0.0",
  "size": 764,
  "file": "firmware_100ms.bin",
  "sha256": "ebc677b065ef35a59aa350dbd7cd93bce679d91e7cfa9eab331392d2e0d7e798"
}
```

`size` là số byte thực tế, từ 8 đến 65536 cho STM32F103C8. Chấp nhận tên trường
`size_bytes` khi không có `size`. Giới hạn manifest là 4096 byte.
`file` có thể là đường dẫn tương đối tính từ manifest, đường dẫn bắt đầu bằng `/`,
hoặc URL HTTP/HTTPS tuyệt đối. Tên đường dẫn phải kết thúc bằng `.bin`; URL có
khoảng trắng phải được percent-encode. Dùng URL trực tiếp, máy chủ trả HTTP 200;
thư viện từ chối phản hồi chuyển hướng và phản hồi tải dở HTTP 206.

`sha256` là tùy chọn để tương thích manifest cũ. Khi có, thư viện bắt buộc kiểm
tra đủ 64 ký tự hex và đối chiếu hash của **file đọc lại từ LittleFS**. ESP32
luôn tính và lưu hash để kiểm tra lại trước khi nạp STM32, kể cả khi manifest
thiếu `sha256`. Với HTTPS, truyền CA PEM làm tham số thứ ba
của `download()` và đồng bộ thời gian hệ thống trước khi tải.

Kiểm tra dữ liệu phát hành trên PowerShell sau mỗi lần build:

```powershell
(Get-Item -LiteralPath '.\firmware_100ms.bin').Length
(Get-FileHash -LiteralPath '.\firmware_100ms.bin' -Algorithm SHA256).Hash.ToLowerInvariant()
```

File tải dở nằm ở `/stm32_firmware.bin.part`. Khi sai độ dài, sai hash hoặc lỗi
kết nối/ghi, file tạm được xóa và giữ file hoàn chỉnh cũ. Sau khi xác minh, thư
viện đổi tên file tạm thành `/stm32_firmware.bin`. Cần chỗ trống cho cả file cũ,
file mới và ít nhất 4 KiB dự phòng. Với dữ liệu LittleFS cần bảo toàn, dùng
`begin(false)` để tránh format nếu mount thất bại.

API mạng tham khảo [ESP HTTP Client của Espressif](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/protocols/esp_http_client.html).
