#ifndef FIRMWARE_DOWNLOAD_H
#define FIRMWARE_DOWNLOAD_H

#include <Arduino.h>
#include <FS.h>

struct FirmwareInfo {
    String board;
    String version;
    String file;
    size_t size = 0;
};

// Ghi theo từng khối, rồi kiểm tra file và lưu SHA-256 trước khi cho phép nạp.
class FirmwareStaging {
public:
    static constexpr size_t minSize = 8;
    static constexpr size_t maxSize = 64U * 1024U; // STM32F103C8.

    ~FirmwareStaging();
    bool begin(String &error, size_t expectedSize = 0);
    bool write(const uint8_t *data, size_t size, String &error);
    bool commit(String &error, const String &expectedSha256 = String());
    size_t size() const { return received_; }
    void discard();

private:
    fs::File file_;
    bool active_ = false;
    size_t received_ = 0;
    size_t expectedSize_ = 0;
};

// API đồng bộ: gọi từ loop() hoặc một task riêng, không gọi từ ISR.
// Chỉ một lượt tải được sử dụng LittleFS và các đường dẫn này tại một thời điểm.
class FirmwareDownloader {
public:
    // Mặc định không format khi mount thất bại để bảo toàn dữ liệu đã lưu.
    bool begin(bool formatOnFail = false);
    bool connectWiFi(const char *ssid, const char *password,
                     uint32_t timeoutMs = 15000);

    // Wi-Fi và LittleFS phải sẵn sàng trước khi gọi.
    // HTTPS cần chứng chỉ CA PEM và đồng hồ hệ thống đã được đồng bộ.
    // info chỉ được gán khi file đã tải và xác minh thành công.
    bool download(const char *manifestUrl, FirmwareInfo &info,
                  const char *rootCa = nullptr);

    // Trả file đã đối chiếu với SHA-256 lưu khi nhận firmware, đặt con trỏ về đầu.
    fs::File openForFlashing();

    const String &lastError() const { return error_; }
    const char *filePath() const;

private:
    String error_;
    bool mounted_ = false;
};

#endif
