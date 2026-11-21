#ifndef APP_IPC_H
#define APP_IPC_H
#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "menu.h"

typedef enum { DISPLAY_MSG_SENSOR, DISPLAY_MSG_RTC } DisplayMsgKind;
typedef struct {
    DisplayMsgKind kind;
    MenuSensor sensor;
    int32_t value;
    uint8_t valid;
    MenuDateTime rtc;
} DisplayMessage;

typedef struct {
    MenuLimit limit;
    int32_t value;
} ThresholdRequest;

extern QueueHandle_t g_display_queue;
extern QueueHandle_t g_rtc_request_queue;
extern QueueHandle_t g_limit_request_queue;

// Goi 1 lan trong main, truoc khi tao Task. 0=OK
uint8_t App_IPC_Init(void);

// Cac task cam bien / RTC gui du lieu vao OLED Task.
BaseType_t Display_PostSensor(MenuSensor id, int32_t value, uint8_t valid);
BaseType_t Display_PostRTC(const MenuDateTime *t);

// Gui YEU CAU tu menu, chua dong nghia da ghi RTC/Flash.
BaseType_t Display_RequestRTC(const MenuDateTime *t);
BaseType_t Display_RequestLimit(MenuLimit id, int32_t value);

#endif