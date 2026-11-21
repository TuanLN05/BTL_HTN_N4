#include <Arduino.h>
#include <WiFi.h>
#include "modeBoot.h"
#include "stm32Flash.h"
#include "webControl.h"
#include <freertos/semphr.h>
#include <utility>

// Điền thông tin mạng và IP LAN của máy tính chạy máy chủ firmware.
static const char *WIFI_SSID = "Tuanzzz";
static const char *WIFI_PASSWORD = "22222222";
static FirmwareStatus status;
static SemaphoreHandle_t jobDone;
// Worker ghi kết quả trước khi báo jobDone. Chỉ loop() cập nhật status.
static std::unique_ptr<uint8_t[]> uploadedImage;
static size_t uploadedSize;
static String result;
static bool succeeded;

static void runFirmwareJob(void *) {
  succeeded = flashSTM32(uploadedImage.get(), uploadedSize, &result);
  if (succeeded) {
    result = String("Đã nạp và đọc lại xác minh ") + uploadedSize + " byte. STM32 đã được reset.";
  }
  uploadedImage.reset();
  xSemaphoreGive(jobDone);
  vTaskDelete(nullptr);
}

static bool startJob(std::unique_ptr<uint8_t[]> image, size_t size) {
  if (status.busy) {
    Serial.println("ESP32 dang xu ly firmware.");
    return false;
  }
  uploadedImage = std::move(image);
  uploadedSize = size;
  ++status.revision;
  result = "";
  if (!jobDone || xTaskCreatePinnedToCore(runFirmwareJob, "stm32-fota", 12288, nullptr, 1,
                                          nullptr, xPortGetCoreID()) != pdPASS) {
    uploadedImage.reset();
    status.message = "ESP32 không đủ bộ nhớ để bắt đầu. Vui lòng thử lại.";
    Serial.println(status.message);
    return false;
  }
  status.busy = true;
  status.message = "Đang nạp và xác minh flash STM32…";
  return true;
}

static void finishJob() {
  if (!status.busy || xSemaphoreTake(jobDone, 0) != pdTRUE) return;
  status.busy = false;
  status.message = result;
  if (!succeeded) Serial.println(result);
}

void setup() {
  Serial.begin(115200);
 
  modeBootInit();
  jobDone = xSemaphoreCreateBinary();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  initWebControl(status, startJob);
}

void loop() {
  finishJob();
  pollWebControl();
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

