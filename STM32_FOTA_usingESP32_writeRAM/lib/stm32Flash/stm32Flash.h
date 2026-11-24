#pragma once

#include <Arduino.h>

constexpr size_t STM32_FLASH_BYTES = 64U * 1024U;

// Firmware .bin cho STM32F103C8, link tại 0x08000000.
// Thành công khi đã đọc lại toàn bộ ảnh và reset sang chế độ ứng dụng.
bool flashSTM32(const uint8_t *data, size_t size, String *error = nullptr);
