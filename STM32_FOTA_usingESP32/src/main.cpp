#include <Arduino.h>
#include <WiFi.h>
#include "modeBoot.h"
#include "stm32Flash.h"
#include "firmwareDownload.h"
#include "webControl.h"
#include <freertos/semphr.h>

// Điền thông tin mạng và IP LAN của máy tính chạy máy chủ firmware.
static const char *WIFI_SSID = "Tuanzzz";
static const char *WIFI_PASSWORD = "22222222";
static const char *MANIFEST_URL = "http://172.20.10.2:8000/manifest.json";
static FirmwareDownloader firmwareDownloader;
static FirmwareStatus status;
static SemaphoreHandle_t jobDone;
// Worker ghi kết quả trước khi báo jobDone. Chỉ loop() cập nhật status.
static FirmwareAction action;
static String result;
static bool succeeded;

static bool handleFirmwareDownload(String &message)
{
  if (String(WIFI_SSID) == "YOUR_WIFI_SSID") {
    message = "Hãy điền Wi-Fi và URL manifest trong src/main.cpp.";
    return false;
  }
  Serial.println("Dang ket noi Wi-Fi va tai firmware STM32...");
  // true cho phép format nếu phân vùng LittleFS chưa được khởi tạo.
  if (!firmwareDownloader.begin(true) ||
      !firmwareDownloader.connectWiFi(WIFI_SSID, WIFI_PASSWORD)) {
    message = firmwareDownloader.lastError();
    return false;
  }
  Serial.printf("Wi-Fi: %s | RSSI: %d dBm\n", WiFi.SSID().c_str(), WiFi.RSSI());
  Serial.printf("ESP32 IP: %s | Gateway: %s | Subnet: %s\n",
                WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(),
                WiFi.subnetMask().toString().c_str());
  Serial.printf("Manifest URL: %s\n", MANIFEST_URL);
  FirmwareInfo info;
  if (!firmwareDownloader.download(MANIFEST_URL, info)) {
    message = firmwareDownloader.lastError();
    return false;
  }
  Serial.printf("Da tai %s v%s: %u byte (%.2f KiB) -> %s\n",
                info.board.c_str(), info.version.c_str(), static_cast<unsigned>(info.size),
                info.size / 1024.0, firmwareDownloader.filePath());
  message = info.file + " · v" + info.version + " · " + info.size + " byte";
  return true;
}

static bool handleFirmwareFlash(String &message)
{
  if (!firmwareDownloader.begin()) {
    message = firmwareDownloader.lastError();
    return false;
  }
  fs::File image = firmwareDownloader.openForFlashing();
  if (!image) {
    message = firmwareDownloader.lastError();
    return false;
  }
  if (!flashSTM32(image, &message)) return false;
  message = String("Đã nạp và đọc lại xác minh ") + image.size() + " byte. STM32 đã được reset.";
  return true;
}

static void runFirmwareJob(void *) {
  succeeded = action == FirmwareAction::download ? handleFirmwareDownload(result) : handleFirmwareFlash(result);
  xSemaphoreGive(jobDone);
  vTaskDelete(nullptr);
}

static bool startJob(FirmwareAction requested) {
  if (status.busy) {
    Serial.println("ESP32 dang xu ly firmware.");
    return false;
  }
  action = requested;
  ++status.revision;
  result = "";
  if (action == FirmwareAction::download) status.image = "";
  if (!jobDone || xTaskCreatePinnedToCore(runFirmwareJob, "stm32-fota", 12288, nullptr, 1,
                                          nullptr, xPortGetCoreID()) != pdPASS) {
    status.message = "ESP32 không đủ bộ nhớ để bắt đầu. Vui lòng thử lại.";
    Serial.println(status.message);
    return false;
  }
  status.busy = true;
  if (action == FirmwareAction::download) {
    status.message = "Đang tải firmware về ESP32…";
  } else {
    status.message = "Đang nạp và xác minh flash STM32…";
  }
  return true;
}

static void finishJob() {
  if (!status.busy || xSemaphoreTake(jobDone, 0) != pdTRUE) return;
  status.busy = false;
  if (succeeded && action == FirmwareAction::download) {
    status.image = result;
    status.message = "Đã tải firmware từ manifest. Có thể nạp bằng lệnh F trên Serial Monitor.";
  } else {
    status.message = result;
    if (!succeeded) Serial.println(result);
  }
}

void setup() {
  Serial.begin(115200);
 
  modeBootInit();
  jobDone = xSemaphoreCreateBinary();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  initWebControl(status, startJob);
  Serial.println("D: tai firmware vao LittleFS. F: nap file da tai vao Flash STM32.");
}

void loop() {
  finishJob();
  pollWebControl();
  switch (Serial.read()) {
    case 'D': case 'd': startJob(FirmwareAction::download); break;
    case 'F': case 'f': startJob(FirmwareAction::flash); break;
  }
  static int lastReading = HIGH;
  static int stableButton = HIGH;
  static unsigned long changedAt = 0;

  const int reading = digitalRead(bootEsp32);
  const unsigned long now = millis();

  if (reading != lastReading) {
    lastReading = reading;
    changedAt = now;
  }

  // GPIO0 is active LOW. Run once per debounced press, even while held.
  if (reading != stableButton && now - changedAt >= 30) {
    stableButton = reading;
    if (stableButton == LOW && !status.busy) {
      Serial.println("BOOT pressed: requesting STM32 bootloader...");
      if (modeBoot()) {
        Serial.println("STM32 bootloader confirmed (ACK 0x79).");
      } else {
        Serial.println("STM32 bootloader not confirmed. Check diagnostics above.");
      }
    }
  }

  delay(1);
}

