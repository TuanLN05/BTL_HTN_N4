#include "modeBoot.h"
#include "uart.h"



void modeBootInit()
{
    pinMode(bootEsp32, INPUT_PULLUP);
    pinMode(STM32_BOOT0, OUTPUT);
    digitalWrite(STM32_BOOT0, LOW);

    digitalWrite(STM32_NRST, LOW); // GPIO26 qua transistor đảo: LOW nhả NRST.
    pinMode(STM32_NRST, OUTPUT);
}

static void pulseReset()
{
    digitalWrite(STM32_NRST, HIGH);
    delay(20);
    digitalWrite(STM32_NRST, LOW);
    delay(100);
}

static void clearSerial2Rx()
{
    while (Serial2.available() > 0)
        Serial2.read();
}

bool modeBoot()
{
    digitalWrite(STM32_BOOT0, HIGH);
    delay(10);
    uartBoot();                     // Cấu hình 8E1 trước khi gửi 0x7F
    pulseReset();

    clearSerial2Rx();               // Bỏ dữ liệu cũ ngay trước khi đồng bộ
    
    Serial2.write(static_cast<uint8_t>(0x7F));
    Serial2.flush();                // Chờ byte đồng bộ truyền xong

    unsigned long start = millis();
    while (millis() - start < 1000)
    {
        if (Serial2.available() > 0)
        {
            int data = Serial2.read();
            Serial.printf("STM32 boot RX: 0x%02X\n", static_cast<unsigned int>(data));

            if (data == 0x79) return true;   // ACK
            if (data == 0x1F) return false;  // NACK
        }
        delay(1);
    }

    return false;
}

void modeApplication()
{
    digitalWrite(STM32_BOOT0, LOW);

    uartApp();                      // Sẵn sàng nhận trước khi STM32 chạy
    clearSerial2Rx();
    delay(10);
    pulseReset();
}
