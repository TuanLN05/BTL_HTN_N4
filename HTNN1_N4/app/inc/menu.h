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

/**
 * @brief Khoi tao trang chu va gan driver OLED, callback cho menu.
 * @param[out] m Trang thai menu.
 * @param[in] display Driver OLED ma menu se ve vao.
 * @param[in] hooks Cac callback luu nguong va ghi RTC, co the la NULL.
 */
void Menu_Init(Menu *m, Sh1106 *display, const MenuHooks *hooks);

/**
 * @brief Cap nhat mot so do va co hop le tren menu.
 * @param[in,out] m Trang thai menu.
 * @param[in] id Chi so so do.
 * @param[in] value Gia tri nhan 10 cho nhiet do, do am va ap suat, hoac ADC tho cho MQ5.
 * @param[in] valid Khac 0 neu mau hop le.
 */
void Menu_SetSensor(Menu *m, MenuSensor id, int32_t value, uint8_t valid);

/**
 * @brief Cap nhat ngay gio hien thi trong menu.
 * @param[in,out] m Trang thai menu.
 * @param[in] date_time Ngay gio can hien thi. Dung NULL de danh dau RTC khong hop le.
 */
void Menu_SetRTC(Menu *m, const MenuDateTime *date_time);

/**
 * @brief Nap nguong hien thi va gioi han no vao mien gia tri cho phep.
 * @param[in,out] m Trang thai menu.
 * @param[in] id Chi so nguong.
 * @param[in] value Nguong moi theo don vi cua so do.
 * @note Ham nay khong goi callback luu Flash.
 */
void Menu_SetThreshold(Menu *m, MenuLimit id, int32_t value);

/**
 * @brief Doc gia tri nguong dang duoc menu su dung.
 * @param[in] m Trang thai menu.
 * @param[in] id Chi so nguong.
 * @return Gia tri nguong, hoac 0 neu menu hay chi so khong hop le.
 */
int32_t Menu_GetThreshold(const Menu *m, MenuLimit id);

/**
 * @brief Cap nhat cac canh bao do TaskAlarm cung cap va chon popup neu can.
 * @param[in,out] m Trang thai menu.
 * @param[in] alarms Mat na canh bao, moi bit tuong ung mot MenuLimit.
 */
void Menu_SetAlarmMask(Menu *m, uint8_t alarms);

/**
 * @brief Xu ly nut nhan de dieu huong, chinh gia tri hoac xac nhan luu.
 * @param[in,out] m Trang thai menu.
 * @param[in] event Su kien nut da qua chong doi.
 */
void Menu_HandleButton(Menu *m, ButtonEvent event);

/**
 * @brief Ve trang hien tai hoac popup vao framebuffer khi giao dien thay doi.
 * @param[in,out] m Trang thai menu va framebuffer.
 * @retval true Da ve khung hinh moi.
 * @retval false Khong co khung can ve hoac dau vao khong hop le.
 */
bool Menu_Draw(Menu *m);

#ifdef __cplusplus
}
#endif

#endif
