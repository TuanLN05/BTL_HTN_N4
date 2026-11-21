#pragma once

#include <Arduino.h>

enum class FirmwareAction { download, flash };

struct FirmwareStatus {
  bool busy = false;
  unsigned revision = 0;
  String image;
  String message = "Chọn file .bin để nạp vào STM32.";
};

// Các hàm này và callback start chạy trên Arduino loop.
void initWebControl(FirmwareStatus &status, bool (*start)(FirmwareAction));
void pollWebControl();
