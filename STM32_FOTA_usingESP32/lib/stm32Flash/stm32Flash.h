#pragma once

#include <FS.h>

// image là file .bin đã kiểm tra SHA-256, link tại 0x08000000 cho STM32F103C8.
// Thành công khi đã đọc lại toàn bộ ảnh và reset sang chế độ ứng dụng.
bool flashSTM32(fs::File &image, String *error = nullptr);
