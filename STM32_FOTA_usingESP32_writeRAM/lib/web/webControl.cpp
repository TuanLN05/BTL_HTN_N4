#include "webControl.h"
#include "stm32Flash.h"
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include <algorithm>
#include <cstring>
#include <new>
#include <utility>

namespace {
extern const char controlPage[] asm("_binary_control_html_start");
WebServer server(80);
String token;
IPAddress servedIP;
const char *host = "stm32-fota";
std::unique_ptr<uint8_t[]> incoming;
size_t uploadCapacity = 0;
size_t uploadSize = 0;
String uploadName;
String uploadError;
bool uploadComplete = false;

String randomKey() {
  uint8_t bytes[12];
  esp_fill_random(bytes, sizeof(bytes));
  char key[2 * sizeof(bytes) + 1];
  for (size_t i = 0; i < sizeof(bytes); ++i) {
    snprintf(key + 2 * i, 3, "%02x", bytes[i]);
  }
  return String(key);
}

void sendPageHeaders() {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Frame-Options", "DENY");
}

String htmlText(String text) {
  text.replace("&", "&amp;");
  text.replace("<", "&lt;");
  text.replace(">", "&gt;");
  return text;
}

String ticket(const FirmwareStatus &status) {
  return token + "-" + String(status.revision);
}

bool hasValidTicket(const FirmwareStatus &status) {
  return token.length() == 24 && server.arg("ticket") == ticket(status);
}

// WebServer gọi các sự kiện file trước khi xử lý POST. Gom đủ file trong RAM.
void receiveUpload(const FirmwareStatus &status) {
  HTTPUpload &file = server.upload();
  switch (file.status) {
    case UPLOAD_FILE_START: {
      if (status.busy || !hasValidTicket(status)) return;
      String extension = file.filename.substring(file.filename.lastIndexOf('.') + 1);
      extension.toLowerCase();
      if (uploadName.length() || uploadError.length() || file.name != "firmware" ||
          file.filename.length() > 255 || file.filename.lastIndexOf('.') <= 0 || extension != "bin") {
        uploadError = "Hãy chọn đúng một file .bin.";
        return;
      }
      uploadName = file.filename;
      const long bodySize = server.header("Content-Length").toInt();
      if (bodySize <= 0) {
        uploadError = "Không xác định được kích thước lượt gửi firmware.";
        return;
      }
      // Content-Length gồm cả phần bao multipart. File .bin luôn nhỏ hơn phần thân POST.
      uploadCapacity = std::min(static_cast<size_t>(bodySize), STM32_FLASH_BYTES);
      incoming.reset(new (std::nothrow) uint8_t[uploadCapacity]);
      if (!incoming) uploadError = "ESP32 không đủ RAM để nhận file firmware.";
      break;
    }
    case UPLOAD_FILE_WRITE:
      if (!incoming || uploadError.length() || !file.currentSize) return;
      if (file.currentSize > uploadCapacity - uploadSize) {
        uploadError = "File .bin vượt quá giới hạn 64 KiB hoặc lượt gửi sai kích thước.";
        incoming.reset();
        return;
      }
      memcpy(incoming.get() + uploadSize, file.buf, file.currentSize);
      uploadSize += file.currentSize;
      break;
    case UPLOAD_FILE_END:
      uploadComplete = incoming && uploadSize == file.totalSize;
      break;
    case UPLOAD_FILE_ABORTED:
      incoming.reset();
      uploadComplete = false;
      break;
  }
}
} // namespace

void initWebControl(FirmwareStatus &status, WebFlashHandler flash) {
  token = randomKey();
  if (token.length() != 24) {
    Serial.println("Khong khoi tao duoc ma bieu mau web.");
    return;
  }
  const char *headers[] = {"Content-Length"};
  server.collectHeaders(headers, 1);
  server.on("/", HTTP_GET, [&status]() {
    sendPageHeaders();
    String page(controlPage);
    page.replace("{{refresh}}", status.busy ? "<meta http-equiv='refresh' content='2'>" : "");
    page.replace("{{ticket}}", ticket(status));
    page.replace("{{busy}}", status.busy ? "disabled" : "");
    page.replace("{{image}}", status.image.length() ? htmlText(status.image) : String("Chưa chọn file."));
    page.replace("{{message}}", htmlText(status.message));
    server.send(200, "text/html; charset=utf-8", page);
  });

  server.on("/upload", HTTP_POST, [&status, flash]() {
    sendPageHeaders();
    if (!hasValidTicket(status) || status.busy) {
      server.send(409, "text/html; charset=utf-8",
                  "<p>Lệnh đã hết hiệu lực. <a href='/'>Xem trạng thái ESP32</a></p>");
      return;
    }
    if (!uploadError.length() && (!uploadComplete || uploadSize == 0)) {
      uploadError = "Hãy chọn một file .bin và gửi lại firmware.";
    }
    if (!uploadError.length()) {
      if (flash(std::move(incoming), uploadSize)) {
        status.image = uploadName + " · " + uploadSize + " byte";
      }
    } else {
      status.message = uploadError;
    }
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
  }, [&status]() { receiveUpload(status); });
}

void pollWebControl() {
  if (token.length() != 24) return;
  const IPAddress ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP() : IPAddress();
  if (ip != servedIP) {
    if (servedIP != IPAddress()) {
      server.stop();
      MDNS.end();
    }
    servedIP = ip;
    if (ip != IPAddress()) {
      server.begin();
      if (MDNS.begin(host)) MDNS.addService("http", "tcp", 80);
      Serial.printf("Web: http://%s/\n", ip.toString().c_str());
    }
  }
  if (servedIP != IPAddress()) {
    server.handleClient();
    // Cả callback file và POST chạy trong handleClient; dọn luôn khi mất kết nối.
    incoming.reset();
    uploadCapacity = 0;
    uploadSize = 0;
    uploadName = "";
    uploadError = "";
    uploadComplete = false;
  }
}
