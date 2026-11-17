#include "app_ipc.h"
#include <string.h>
#include <stddef.h>

QueueHandle_t g_display_queue = NULL;
QueueHandle_t g_rtc_request_queue = NULL;
QueueHandle_t g_limit_request_queue = NULL;

uint8_t App_IPC_Init(void)
{
    // Goi mot lan truoc khi tao task. Moi queue sao chep ca cau truc du lieu.
    // display: du lieu len OLED; rtc_request: yeu cau chinh gio;
    // limit_request: thong bao nguong moi cho task dieu khien.
    g_display_queue = xQueueCreate(16, sizeof(DisplayMessage));
    g_rtc_request_queue = xQueueCreate(2, sizeof(MenuDateTime));
    g_limit_request_queue = xQueueCreate(8, sizeof(ThresholdRequest));
    return (g_display_queue && g_rtc_request_queue && g_limit_request_queue)
           ? 0 : 1;
}

BaseType_t Display_PostSensor(MenuSensor id, int32_t value, uint8_t valid)
{
    // Task cam bien goi ham nay; khong sua truc tiep Menu cua task display.
    // value dung don vi x10 (253 = 25.3), rieng MQ5 la so dem ADC.
    DisplayMessage msg = {0};
    if (!g_display_queue || id >= MENU_SENSOR_COUNT) return pdFALSE;
    msg.kind = DISPLAY_MSG_SENSOR;
    msg.sensor = id;
    msg.value = value;
    msg.valid = valid;
    // Khong cho khi queue day: tra pdFALSE de ben gui biet gui that bai.
    return xQueueSend(g_display_queue, &msg, 0);
}

BaseType_t Display_PostRTC(const MenuDateTime *t)
{
    // Sao chep ngay gio vao message, nen ben gui co the dung lai bien t sau do.
    DisplayMessage msg = {0};
    if (!g_display_queue || !t) return pdFALSE;
    msg.kind = DISPLAY_MSG_RTC;
    msg.rtc = *t;
    return xQueueSend(g_display_queue, &msg, 0);
}

BaseType_t Display_RequestRTC(const MenuDateTime *t)
{
    // Chi gui yeu cau; task RTC phai nhan queue va ghi vao chip RTC that.
    if (!g_rtc_request_queue || !t) return pdFALSE;
    return xQueueSend(g_rtc_request_queue, t, 0);
}

BaseType_t Display_RequestLimit(MenuLimit id, int32_t value)
{
    // Task dieu khien nhan ThresholdRequest de cap nhat nguong dang su dung.
    ThresholdRequest req;
    if (!g_limit_request_queue || id >= MENU_LIMIT_COUNT) return pdFALSE;
    req.limit = id;
    req.value = value;
    return xQueueSend(g_limit_request_queue, &req, 0);
}
