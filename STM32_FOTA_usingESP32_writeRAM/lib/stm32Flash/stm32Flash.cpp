#include "stm32Flash.h"
#include "modeBoot.h"
#include <algorithm>
#include <cstring>

namespace {
constexpr uint32_t FLASH_BASE = 0x08000000;
constexpr size_t PAGE_BYTES = 1024;
constexpr size_t BLOCK_BYTES = 256;
constexpr uint8_t GET = 0x00, GET_ID = 0x02, READ = 0x11, WRITE = 0x31;
constexpr uint8_t ERASE = 0x43, EXT_ERASE = 0x44;

bool receive(uint8_t *data, size_t length, uint32_t timeoutMs = 1000)
{
    const uint32_t start = millis();
    size_t received = 0;
    while (received < length && millis() - start < timeoutMs) {
        int byte = Serial2.read();
        if (byte >= 0) data[received++] = static_cast<uint8_t>(byte);
        else delay(1);
    }
    if (received == length) return true;
    Serial.println("UART timeout.");
    return false;
}

bool ack(uint32_t timeoutMs = 1000)
{
    uint8_t reply;
    if (!receive(&reply, 1, timeoutMs)) return false;
    if (reply == 0x79) return true;
    Serial.printf("Cho ACK 0x79, nhan 0x%02X.\n", reply);
    return false;
}

bool command(uint8_t value)
{
    const uint8_t frame[] = {value, static_cast<uint8_t>(~value)};
    return Serial2.write(frame, sizeof(frame)) == sizeof(frame) && ack();
}

// Địa chỉ, danh sách trang và dữ liệu ghi đều kết thúc bằng XOR theo AN3155.
bool sendWithChecksum(const uint8_t *data, size_t length, uint32_t timeoutMs = 1000)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; ++i) checksum ^= data[i];
    return Serial2.write(data, length) == length && Serial2.write(checksum) == 1 && ack(timeoutMs);
}

bool addressCommand(uint8_t cmd, uint32_t address)
{
    const uint8_t bytes[] = {uint8_t(address >> 24), uint8_t(address >> 16),
                             uint8_t(address >> 8), uint8_t(address)};
    return command(cmd) && sendWithChecksum(bytes, sizeof(bytes));
}

bool identify(uint8_t &eraseCmd)
{
    // GET trả N, phiên bản, N mã lệnh, ACK. GET_ID trả N, N+1 byte PID, ACK.
    uint8_t count, version;
    if (!command(GET) || !receive(&count, 1) || !receive(&version, 1)) return false;
    bool hasRead = false, hasWrite = false, hasId = false;
    for (unsigned i = 0; i < count; ++i) {
        uint8_t cmd;
        if (!receive(&cmd, 1)) return false;
        if (cmd == READ) hasRead = true;
        if (cmd == WRITE) hasWrite = true;
        if (cmd == GET_ID) hasId = true;
        if (cmd == ERASE || cmd == EXT_ERASE) eraseCmd = cmd;
    }
    if (!ack() || !hasRead || !hasWrite || !hasId || !eraseCmd) return false;

    uint8_t pid[2];
    if (!command(GET_ID) || !receive(&count, 1) || count != 1 ||
        !receive(pid, sizeof(pid)) || !ack()) return false;
    Serial.printf("Bootloader 0x%02X, PID 0x%02X%02X.\n", version, pid[0], pid[1]);
    return pid[0] == 0x04 && pid[1] == 0x10; // Họ STM32F1 medium-density.
}

bool eraseImagePages(uint8_t cmd, size_t imageSize)
{
    const size_t pages = (imageSize + PAGE_BYTES - 1) / PAGE_BYTES;
    uint8_t frame[2 + 2 * (STM32_FLASH_BYTES / PAGE_BYTES)];
    size_t used = 0;
    if (cmd == EXT_ERASE) frame[used++] = 0;
    frame[used++] = static_cast<uint8_t>(pages - 1);
    for (size_t page = 0; page < pages; ++page) {
        if (cmd == EXT_ERASE) frame[used++] = 0;
        frame[used++] = static_cast<uint8_t>(page);
    }
    Serial.printf("Xoa %u trang tu 0x08000000.\n", unsigned(pages));
    return command(cmd) && sendWithChecksum(frame, used, 10000);
}

bool writeImage(const uint8_t *image, size_t imageSize)
{
    uint8_t frame[1 + BLOCK_BYTES];
    for (size_t offset = 0; offset < imageSize; offset += BLOCK_BYTES) {
        size_t count = std::min(BLOCK_BYTES, imageSize - offset);
        size_t padded = (count + 3) & ~size_t(3);
        frame[0] = static_cast<uint8_t>(padded - 1);
        memcpy(frame + 1, image + offset, count);
        memset(frame + 1 + count, 0xFF, padded - count);
        if (!addressCommand(WRITE, FLASH_BASE + offset) ||
            !sendWithChecksum(frame, 1 + padded)) {
            Serial.printf("Loi ghi tai 0x%08lX.\n", (unsigned long)(FLASH_BASE + offset));
            return false;
        }
    }
    return true;
}

bool verifyImage(const uint8_t *image, size_t imageSize)
{
    uint8_t actual[128];
    for (size_t offset = 0; offset < imageSize; offset += sizeof(actual)) {
        size_t count = std::min(sizeof(actual), imageSize - offset);
        // READ nhận độ dài - 1 và byte bù, sau đó trả ACK và dữ liệu.
        if (!addressCommand(READ, FLASH_BASE + offset) ||
            !command(static_cast<uint8_t>(count - 1)) || !receive(actual, count) ||
            memcmp(actual, image + offset, count) != 0) {
            Serial.printf("Loi doc/doi chieu tai 0x%08lX.\n", (unsigned long)(FLASH_BASE + offset));
            return false;
        }
    }
    return true;
}
bool flashError(const char *message, String *error)
{
    if (error) *error = message;
    else Serial.println(message);
    return false;
}
} // namespace

bool flashSTM32(const uint8_t *image, size_t size, String *error)
{
    // ESP32 và STM32F103 đều little-endian. MSP có thể bằng đỉnh SRAM.
    uint32_t vectors[2];
    if (!image || size < sizeof(vectors) || size > STM32_FLASH_BYTES) {
        return flashError("File .bin không đọc được hoặc sai kích thước.", error);
    }
    memcpy(vectors, image, sizeof(vectors));
    const uint32_t stack = vectors[0], reset = vectors[1] & ~1U;
    if (stack <= 0x20000000 || stack > 0x20005000 || (stack & 3) != 0 ||
        !(vectors[1] & 1) || reset < FLASH_BASE + sizeof(vectors) || reset >= FLASH_BASE + size) {
        return flashError("Vector table không hợp lệ cho STM32F103C8 tại 0x08000000.", error);
    }
    if (!modeBoot()) {
        return flashError("Không nhận được ACK. Kiểm tra nguồn, UART, BOOT0 và NRST.", error);
    }
    uint8_t eraseCmd = 0;
    if (!identify(eraseCmd)) {
        return flashError("Sai dòng chip hoặc bootloader thiếu lệnh. Đã dừng trước khi xóa.", error);
    }
    if (!eraseImagePages(eraseCmd, size) || !writeImage(image, size) || !verifyImage(image, size)) {
        return flashError("Nạp chưa hoàn tất. Kiểm tra kết nối STM32 rồi thử nạp lại.", error);
    }
    modeApplication();
    Serial.printf("Da xac minh %u byte va reset STM32 ve ung dung.\n", unsigned(size));
    return true;
}
