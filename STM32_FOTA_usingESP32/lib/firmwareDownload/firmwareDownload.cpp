#include "firmwareDownload.h"

#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <cJSON.h>
#include <esp_http_client.h>
#include <http_parser.h>
#include <mbedtls/md.h>
#include <cmath>
#include <memory>

namespace {
constexpr size_t MAX_MANIFEST_SIZE = 4096;
constexpr uint32_t TRANSFER_TIMEOUT_MS = 60000;
constexpr int NETWORK_TIMEOUT_MS = 10000;
constexpr const char *PART_PATH = "/stm32_firmware.bin.part";
constexpr const char *FIRMWARE_PATH = "/stm32_firmware.bin";
constexpr const char *RECEIPT_NAMESPACE = "stm32-fota";
constexpr const char *DIGEST_KEY = "image-sha";

bool validUrl(const String &url)
{
    if (url.length() > 1024 ||
        (!url.startsWith("http://") && !url.startsWith("https://"))) return false;
    for (size_t i = 0; i < url.length(); ++i) {
        if (static_cast<unsigned char>(url[i]) <= 32) return false;
    }
    http_parser_url parsed = {};
    return http_parser_parse_url(url.c_str(), url.length(), 0, &parsed) == 0 &&
           (parsed.field_set & (1U << UF_HOST)) &&
           !(parsed.field_set & ((1U << UF_USERINFO) | (1U << UF_FRAGMENT)));
}

// Các URI tương đối được tính từ thư mục chứa manifest.
String resolveFileUrl(const String &manifestUrl, const String &file)
{
    if (file.startsWith("http://") || file.startsWith("https://")) return file;
    if (file.startsWith("//") || file.indexOf(':') >= 0) return String();

    String base = manifestUrl;
    const int query = base.indexOf('?');
    if (query >= 0) base.remove(query);
    const int authorityEnd = base.indexOf('/', base.indexOf("://") + 3);
    const String origin = authorityEnd < 0 ? base : base.substring(0, authorityEnd);
    if (file.startsWith("/")) return origin + file;
    return (authorityEnd < 0 ? origin + "/" : base.substring(0, base.lastIndexOf('/') + 1)) + file;
}

class HttpSession {
public:
    esp_http_client_handle_t handle = nullptr;
    int contentLength = 0;

    ~HttpSession()
    {
        if (handle) esp_http_client_cleanup(handle);
    }

    bool open(const String &url, const char *rootCa, String &error)
    {
        if (!validUrl(url)) {
            error = "URL không hợp lệ; cần đường dẫn HTTP hoặc HTTPS.";
            return false;
        }
        if (url.startsWith("https://") && (!rootCa || !rootCa[0])) {
            error = "HTTPS cần chứng chỉ CA để xác thực máy chủ.";
            return false;
        }

        esp_http_client_config_t config = {};
        config.url = url.c_str();
        config.cert_pem = rootCa;
        config.timeout_ms = NETWORK_TIMEOUT_MS;
        config.buffer_size = 1024;
        config.disable_auto_redirect = true;
        config.user_agent = "ESP32-STM32-FOTA/1.0";
        startedAt_ = millis();
        handle = esp_http_client_init(&config);
        if (!handle) {
            error = "Không đủ bộ nhớ để tạo kết nối HTTP.";
            return false;
        }
        esp_http_client_set_header(handle, "Accept-Encoding", "identity");
        esp_http_client_set_header(handle, "Cache-Control", "no-cache");
        const esp_err_t opened = esp_http_client_open(handle, 0);
        if (opened != ESP_OK) {
            error = String("Không thể kết nối máy chủ ") + url + ": " + esp_err_to_name(opened);
            return false;
        }
        contentLength = esp_http_client_fetch_headers(handle);
        if (contentLength < 0) {
            error = "Không thể đọc phản hồi HTTP hoặc đã hết thời gian chờ.";
            return false;
        }
        const int status = esp_http_client_get_status_code(handle);
        if (status != 200) {
            error = String("Máy chủ trả về HTTP ") + status + ". Cần URL trực tiếp đến file.";
            return false;
        }
        return true;
    }

    int read(char *buffer, size_t capacity, String &error)
    {
        if (millis() - startedAt_ >= TRANSFER_TIMEOUT_MS) {
            error = "Tải dữ liệu quá thời gian chờ 60 giây.";
            return -1;
        }
        if (WiFi.status() != WL_CONNECTED) {
            error = "Mất kết nối Wi-Fi khi đang tải dữ liệu.";
            return -1;
        }
        const int count = esp_http_client_read(handle, buffer, capacity);
        if (count < 0 || (count == 0 && !esp_http_client_is_complete_data_received(handle))) {
            error = "Kết nối tải bị ngắt, thiếu dữ liệu hoặc hết thời gian chờ.";
            return -1;
        }
        return count;
    }

private:
    uint32_t startedAt_ = 0;
};

bool textMember(cJSON *root, const char *key, String &value, size_t maxLength)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(item) || !item->valuestring) return false;
    value = item->valuestring;
    value.trim();
    return !value.isEmpty() && value.length() <= maxLength;
}

bool readManifest(const String &url, const char *rootCa, FirmwareInfo &info,
                  String &fileUrl, String &expectedSha256, String &error)
{
    HttpSession http;
    if (!http.open(url, rootCa, error)) return false;
    if (http.contentLength > static_cast<int>(MAX_MANIFEST_SIZE)) {
        error = "Manifest vượt quá giới hạn 4096 byte.";
        return false;
    }

    String body;
    char buffer[512];
    int count;
    while ((count = http.read(buffer, sizeof(buffer), error)) > 0) {
        if (body.length() + count > MAX_MANIFEST_SIZE) {
            error = "Manifest vượt quá giới hạn 4096 byte.";
            return false;
        }
        if (!body.concat(buffer, count)) {
            error = "Không đủ RAM để đọc manifest.";
            return false;
        }
        delay(1);
    }
    if (count < 0) return false;

    const char *parseEnd = nullptr;
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
        cJSON_ParseWithLengthOpts(body.c_str(), body.length() + 1, &parseEnd, true),
        cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get()) || parseEnd != body.c_str() + body.length()) {
        error = "Manifest không phải một đối tượng JSON hợp lệ.";
        return false;
    }
    if (!textMember(root.get(), "board", info.board, 64) ||
        !textMember(root.get(), "version", info.version, 64) ||
        !textMember(root.get(), "file", info.file, 512)) {
        error = "Manifest cần các chuỗi board, version và file không rỗng.";
        return false;
    }
    if (info.board != "STM32F103C8") {
        error = "Firmware không dành cho bo STM32F103C8.";
        return false;
    }

    const cJSON *size = cJSON_GetObjectItemCaseSensitive(root.get(), "size");
    if (!size) size = cJSON_GetObjectItemCaseSensitive(root.get(), "size_bytes");
    if (!cJSON_IsNumber(size) || !std::isfinite(size->valuedouble) ||
        size->valuedouble < FirmwareStaging::minSize || size->valuedouble > FirmwareStaging::maxSize ||
        std::floor(size->valuedouble) != size->valuedouble) {
        error = "Kích thước firmware phải là số byte nguyên từ 8 đến 65536.";
        return false;
    }
    info.size = static_cast<size_t>(size->valuedouble);

    const cJSON *sha = cJSON_GetObjectItemCaseSensitive(root.get(), "sha256");
    if (sha) {
        if (!textMember(root.get(), "sha256", expectedSha256, 64) || expectedSha256.length() != 64) {
            error = "sha256 phải gồm đúng 64 ký tự hexadecimal.";
            return false;
        }
        expectedSha256.toLowerCase();
        for (size_t i = 0; i < expectedSha256.length(); ++i) {
            const char c = expectedSha256[i];
            if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f')) {
                error = "sha256 chứa ký tự hexadecimal không hợp lệ.";
                return false;
            }
        }
    }

    fileUrl = resolveFileUrl(url, info.file);
    String path = fileUrl;
    const int query = path.indexOf('?');
    if (query >= 0) path.remove(query);
    path.toLowerCase();
    if (!validUrl(fileUrl) || !path.endsWith(".bin") ||
        (url.startsWith("https://") && !fileUrl.startsWith("https://"))) {
        error = "Trường file cần trỏ đến file .bin; HTTPS không được chuyển xuống HTTP.";
        return false;
    }
    return true;
}

bool hashFile(File &file, String &hex, String &error)
{
    mbedtls_md_context_t context;
    mbedtls_md_init(&context);
    bool ok = mbedtls_md_setup(&context, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0) == 0 &&
              mbedtls_md_starts(&context) == 0;
    uint8_t buffer[1024];
    size_t remaining = file.size();
    while (ok && remaining > 0) {
        const size_t count = file.read(buffer, sizeof(buffer));
        ok = count > 0 && count <= sizeof(buffer) && count <= remaining &&
             mbedtls_md_update(&context, buffer, count) == 0;
        if (ok) remaining -= count;
        delay(1);
    }
    uint8_t digest[32];
    ok = ok && mbedtls_md_finish(&context, digest) == 0;
    mbedtls_md_free(&context);
    if (!ok) {
        error = "Không thể tính SHA-256 của file đã lưu.";
        return false;
    }
    char text[65];
    for (size_t i = 0; i < sizeof(digest); ++i) {
        snprintf(text + 2 * i, 3, "%02x", digest[i]);
    }
    hex = text;
    return true;
}
} // namespace

FirmwareStaging::~FirmwareStaging()
{
    discard();
}

void FirmwareStaging::discard()
{
    file_.close();
    if (active_) LittleFS.remove(PART_PATH);
    active_ = false;
}

bool FirmwareStaging::begin(String &error, size_t expectedSize)
{
    expectedSize_ = expectedSize;
    received_ = 0;
    file_ = LittleFS.open(PART_PATH, FILE_WRITE);
    if (!file_ || file_.isDirectory()) {
        error = "Không tạo được file firmware tạm trong LittleFS.";
        return false;
    }
    active_ = true;
    file_.flush();
    if (LittleFS.totalBytes() - LittleFS.usedBytes() < expectedSize + 4096) {
        error = "LittleFS không đủ chỗ trống để tải file mới và giữ file cũ.";
        return false;
    }
    return true;
}

bool FirmwareStaging::write(const uint8_t *data, size_t size, String &error)
{
    const size_t limit = expectedSize_ ? expectedSize_ : maxSize;
    if (size > limit - received_) {
        error = expectedSize_ ? "File .bin lớn hơn kích thước khai báo trong manifest."
                              : "File .bin vượt quá giới hạn 64 KiB.";
        return false;
    }
    if (file_.write(data, size) != size) {
        error = "Không ghi đủ dữ liệu firmware vào LittleFS.";
        return false;
    }
    received_ += size;
    return true;
}

bool FirmwareStaging::commit(String &error, const String &expectedSha256)
{
    if (expectedSize_ && received_ != expectedSize_) {
        error = "File .bin bị thiếu dữ liệu so với size trong manifest.";
        return false;
    }
    if (received_ < minSize) {
        error = "File .bin phải có ít nhất 8 byte.";
        return false;
    }
    file_.flush();
    file_.close();
    file_ = LittleFS.open(PART_PATH, FILE_READ);
    if (!file_ || file_.size() != received_) {
        error = "Kích thước file đã lưu trong LittleFS không hợp lệ.";
        return false;
    }
    String digest;
    if (!hashFile(file_, digest, error)) return false;
    file_.close();
    if (!expectedSha256.isEmpty() && expectedSha256 != digest) {
        error = "SHA-256 không khớp manifest. File firmware đã bị từ chối.";
        return false;
    }
    Preferences receipt;
    if (!receipt.begin(RECEIPT_NAMESPACE, false)) {
        error = "Không mở được NVS để lưu SHA-256.";
        return false;
    }
    if (!LittleFS.rename(PART_PATH, FIRMWARE_PATH)) {
        error = "Không thể đổi tên file firmware đã xác minh.";
        return false;
    }
    active_ = false;
    if (receipt.putString(DIGEST_KEY, digest) != 64) {
        error = "Đã lưu file nhưng chưa lưu được SHA-256. Hãy gửi lại firmware.";
        return false;
    }
    return true;
}

const char *FirmwareDownloader::filePath() const
{
    return FIRMWARE_PATH;
}

bool FirmwareDownloader::begin(bool formatOnFail)
{
    error_ = "";
    mounted_ = LittleFS.begin(formatOnFail);
    if (!mounted_) error_ = "Không mount được LittleFS. Kiểm tra phân vùng hoặc format lần đầu.";
    return mounted_;
}

bool FirmwareDownloader::connectWiFi(const char *ssid, const char *password, uint32_t timeoutMs)
{
    error_ = "";
    if (!ssid || !ssid[0] || timeoutMs == 0) {
        error_ = "Cần cấu hình tên Wi-Fi và thời gian chờ hợp lệ.";
        return false;
    }
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid) return true;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < timeoutMs) delay(20);
    if (WiFi.status() != WL_CONNECTED) {
        error_ = "Không kết nối được Wi-Fi trong thời gian chờ.";
        return false;
    }
    return true;
}

bool FirmwareDownloader::download(const char *manifestUrl, FirmwareInfo &info, const char *rootCa)
{
    error_ = "";
    if (!mounted_ || WiFi.status() != WL_CONNECTED || !manifestUrl) {
        error_ = "Cần khởi tạo LittleFS, kết nối Wi-Fi và cung cấp URL manifest trước khi tải.";
        return false;
    }

    FirmwareInfo candidate;
    String fileUrl;
    String expectedSha256;
    if (!readManifest(String(manifestUrl), rootCa, candidate, fileUrl, expectedSha256, error_)) return false;
    HttpSession http;
    if (!http.open(fileUrl, rootCa, error_)) return false;
    if (http.contentLength > 0 && static_cast<size_t>(http.contentLength) != candidate.size) {
        error_ = candidate.file + ": Content-Length = " + http.contentLength +
                 " byte, size trong manifest = " + candidate.size + " byte.";
        return false;
    }
    FirmwareStaging staged;
    if (!staged.begin(error_, candidate.size)) return false;

    char buffer[1024];
    int count;
    while ((count = http.read(buffer, sizeof(buffer), error_)) > 0) {
        if (!staged.write(reinterpret_cast<const uint8_t *>(buffer), count, error_)) return false;
        delay(1);
    }
    if (count < 0 || !staged.commit(error_, expectedSha256)) return false;
    info = candidate;
    return true;
}

fs::File FirmwareDownloader::openForFlashing()
{
    error_ = "";
    fs::File image = LittleFS.open(filePath(), FILE_READ);
    Preferences receipt;
    if (!image || !receipt.begin(RECEIPT_NAMESPACE, true)) {
        error_ = "Chưa có firmware và SHA-256 đã lưu. Gửi D trước khi nạp.";
        return {};
    }
    const String expected = receipt.getString(DIGEST_KEY, "");
    if (expected.length() != 64 || image.size() < FirmwareStaging::minSize ||
        image.size() > FirmwareStaging::maxSize) {
        error_ = "File hoặc SHA-256 đã lưu không hợp lệ. Gửi lại D.";
        return {};
    }
    String actual;
    if (!hashFile(image, actual, error_)) return {};
    if (actual != expected || !image.seek(0)) {
        error_ = "File trong LittleFS không khớp bản đã tải hoặc không đọc được. Gửi lại D.";
        return {};
    }
    return image;
}
