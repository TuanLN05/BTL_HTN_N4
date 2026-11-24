#pragma once

#include <Arduino.h>
#include <memory>

struct FirmwareStatus {
  bool busy = false;
  unsigned revision = 0;
  String image;
  String message = "Chọn file .bin để nạp vào STM32.";
};

// Callback chạy trên Arduino loop và nhận quyền sở hữu bộ đệm firmware.
using WebFlashHandler = bool (*)(std::unique_ptr<uint8_t[]> image, size_t size);
void initWebControl(FirmwareStatus &status, WebFlashHandler flash);
void pollWebControl();
