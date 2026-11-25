#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "app_data.h"
#include "sh1106.h"
#include "button.h"

// Khi user an SAVE:
// - threshold_saved: luu Flash, tra 0 thanh cong, khac 0 la loi.
// - rtc_write: yeu cau driver RTC thuc hien ghi thoi gian.
// rtc_write tra ve 0 neu ghi/chap nhan thanh cong, != 0 neu loi.
// Neu chua noi callback RTC, SAVE se bao chua san sang.
typedef struct {
    int (*threshold_saved)(MenuLimit id, int32_t new_value, void *user);
    int (*rtc_write)(const MenuDateTime *new_time, void *user);
    void *user;
} MenuHooks;

typedef enum {
    MENU_PAGE_HOME = 0,
    MENU_PAGE_MAIN,
    MENU_PAGE_LIMIT_LIST,
    MENU_PAGE_LIMIT_CHANGE,
    MENU_PAGE_LIMIT_CONFIRM,
    MENU_PAGE_RTC_LIST,
    MENU_PAGE_RTC_CHANGE,
    MENU_PAGE_ALERTS
} MenuPage;

typedef struct {
    Sh1106 *display;
    MenuHooks hooks;
    int32_t sensor[MENU_SENSOR_COUNT];
    uint8_t valid_mask;
    int32_t threshold[MENU_LIMIT_COUNT];
    MenuDateTime rtc;
    MenuDateTime edit_rtc;
    uint8_t rtc_valid;
    uint8_t alarm_mask;
    uint8_t popup_visible;
    uint8_t popup_alarm_index;
    uint8_t threshold_save_failed;
    uint8_t rtc_save_failed;
    uint8_t dirty;
    MenuPage page;
    uint8_t selected_main;
    uint8_t selected_limit;
    uint8_t selected_rtc;
    uint8_t selected_action;
    MenuLimit editing_limit;
    uint8_t editing_rtc_field;
    int32_t edit_limit_value;
} Menu;

#ifdef __cplusplus
extern "C" {
#endif

void Menu_Init(Menu *m, Sh1106 *display, const MenuHooks *hooks);
void Menu_SetSensor(Menu *m, MenuSensor id, int32_t value, uint8_t valid);
void Menu_SetRTC(Menu *m, const MenuDateTime *date_time);
// Nap nguong tu bo nho (khong goi callback saved).
void Menu_SetThreshold(Menu *m, MenuLimit id, int32_t value);
int32_t Menu_GetThreshold(const Menu *m, MenuLimit id);
void Menu_SetAlarmMask(Menu *m, uint8_t alarms);
void Menu_HandleButton(Menu *m, ButtonEvent event);
// Ve vao framebuffer neu co thay doi. TaskUI gui tung doan qua I2C.
bool Menu_Draw(Menu *m);

#ifdef __cplusplus
}
#endif

#endif
