#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "sh1106.h"
#include "button.h"

// Giao dien nhan du lieu va thao tac phan cung qua callback.
// Sensor: cac gia tri co phan thap phan nhan 10.
// Vi du 253 => 25.3 do C; 10132 => 1013.2 hPa.
// MQ5 la gia tri ADC THO, khong phai nong do ppm.
typedef enum {
    MENU_AHT_TEMP = 0,
    MENU_AHT_HUM,
    MENU_BMP_TEMP,
    MENU_BMP_PRESS,
    MENU_MQ5_ADC,
    MENU_SENSOR_COUNT
} MenuSensor;

// Nguong nhiet do dung AHT20, BMP280 do ap suat.
typedef enum {
    MENU_LIMIT_TEMP = 0,
    MENU_LIMIT_HUM,
    MENU_LIMIT_PRESS,
    MENU_LIMIT_GAS,
    MENU_LIMIT_COUNT
} MenuLimit;

// Bit tuong ung voi limit.
#define MENU_ALARM_TEMP   (1 << MENU_LIMIT_TEMP)
#define MENU_ALARM_HUM    (1 << MENU_LIMIT_HUM)
#define MENU_ALARM_PRESS  (1 << MENU_LIMIT_PRESS)
#define MENU_ALARM_GAS    (1 << MENU_LIMIT_GAS)

typedef struct {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
} MenuDateTime;

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

void Menu_Init(Menu *m, Sh1106 *display, const MenuHooks *hooks);
void Menu_SetSensor(Menu *m, MenuSensor id, int32_t value, uint8_t valid);
void Menu_SetRTC(Menu *m, const MenuDateTime *date_time);
// Nap nguong tu bo nho (khong goi callback saved).
void Menu_SetThreshold(Menu *m, MenuLimit id, int32_t value);
int32_t Menu_GetThreshold(const Menu *m, MenuLimit id);
uint8_t Menu_GetAlarmMask(const Menu *m);
void Menu_HandleButton(Menu *m, ButtonEvent event);
// Goi trong loop / task; chi gui I2C khi dirty=1.
// 1 = da ve; 0 = khong doi; -1 = I2C loi (giu dirty de thu lai).
int Menu_Render(Menu *m);

#endif