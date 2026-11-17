#include "menu.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// Moi bit trong valid_mask/alarm_mask ung voi mot cam bien/mot loai canh bao.
#define BIT(n) ((uint8_t)(1 << (n)))
#define SENSOR_ALL_VALID ((uint8_t)((1 << MENU_SENSOR_COUNT) - 1))
#define LIST_VISIBLE 6

// Cac nguong chi la MAU DE THU GIAO DIEN, KHONG la nguong an toan.
// Don vi x10 tru GAS = so dem ADC 12 bit.
static const int32_t limit_min[MENU_LIMIT_COUNT] = {-400, 0, 3000, 0};
static const int32_t limit_max[MENU_LIMIT_COUNT] = { 850, 1000, 12000, 4095};
static const int32_t limit_step[MENU_LIMIT_COUNT] = {5, 10, 10, 50};
static const MenuSensor limit_source[MENU_LIMIT_COUNT] = {
    MENU_AHT_TEMP, MENU_AHT_HUM, MENU_BMP_PRESS, MENU_MQ5_ADC
};
static const char *const limit_names[] = {
    "AHT TEMP", "AHT HUM", "BMP PRESS", "MQ5 GAS ADC"
};
static const char *const rtc_names[] = {
    "DAY", "MONTH", "YEAR", "HOUR", "MINUTE", "SECOND"
};
static const char *const main_items[] = {
    "SET THRESHOLDS", "SET RTC", "ALERT STATUS", "BACK TO HOME"
};
static const char *const threshold_items[] = {
    "AHT TEMP", "AHT HUMIDITY", "BMP PRESSURE", "MQ5 GAS", "BACK"
};

// Ham tien ich chi ve ky tu 5x7; 1 row=8 pixel.
static void line(Menu *m, uint8_t row, const char *str)
{
    SH1106_DrawText(m->display, 0, (uint8_t)(row * 8), str);
}

static uint8_t count_bits(uint8_t v)
{
    uint8_t n = 0;
    while (v) { n += (uint8_t)(v & 1); v >>= 1; }
    return n;
}

static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t leap;
    if (month < 1 || month > 12) return 31;
    leap = (uint8_t)((year % 4 == 0) &&
                     ((year % 100 != 0) || (year % 400 == 0)));
    return (month == 2 && leap) ? 29 : days[month - 1];
}

static uint8_t valid_rtc(const MenuDateTime *d)
{
    return d && d->year >= 2000 && d->year <= 2099 &&
        d->month >= 1 && d->month <= 12 &&
        d->day >= 1 && d->day <= days_in_month(d->year, d->month) &&
        d->hour <= 23 && d->minute <= 59 && d->second <= 59;
}

// Khong dung so float trong giao dien (nhanh, gon cho STM32F103).
// 253 -> "25.3", -32 -> "-3.2"
static void fmt_x10(char *buf, size_t len, int32_t scaled)
{
    uint32_t abs_value = (uint32_t)(scaled < 0 ? -(int64_t)scaled : scaled);
    (void)snprintf(buf, len, "%s%lu.%lu", scaled < 0 ? "-" : "",
                   (unsigned long)(abs_value / 10),
                   (unsigned long)(abs_value % 10));
}

static void fmt_sensor(const Menu *m, MenuSensor s, char *buf, size_t len)
{
    if (!(m->valid_mask & BIT(s))) {
        (void)snprintf(buf, len, "--");
    } else if (s == MENU_MQ5_ADC) {
        (void)snprintf(buf, len, "%ld", (long)m->sensor[s]);
    } else {
        fmt_x10(buf, len, m->sensor[s]);
    }
}

static void fmt_limit(MenuLimit k, int32_t scaled, char *buf, size_t len)
{
    if (k == MENU_LIMIT_GAS) {
        (void)snprintf(buf, len, "%ld ADC", (long)scaled);
    } else {
        char number[16];
        fmt_x10(number, sizeof(number), scaled);
        (void)snprintf(buf, len, "%s%s", number,
                       k == MENU_LIMIT_HUM ? "%" :
                       k == MENU_LIMIT_PRESS ? "hPa" : "C");
    }
}

static uint8_t high_priority_alarm(uint8_t alarms)
{
    // GAS uu tien cao nhat, sau do nhiet do, do am, ap suat.
    static const uint8_t order[] = {
        MENU_LIMIT_GAS, MENU_LIMIT_TEMP, MENU_LIMIT_HUM, MENU_LIMIT_PRESS
    };
    uint8_t i;
    for (i = 0; i < MENU_LIMIT_COUNT; ++i)
        if (alarms & BIT(order[i])) return order[i];
    return MENU_LIMIT_TEMP;
}

static void recompute_alarms(Menu *m)
{
    // Chi so sanh nguong khi mau cam bien hop le; vuot hoac bang nguong la bao dong.
    uint8_t i, alarms = 0;
    for (i = 0; i < MENU_LIMIT_COUNT; ++i) {
        MenuSensor s = limit_source[i];
        if ((m->valid_mask & BIT(s)) && m->sensor[s] >= m->threshold[i])
            alarms |= BIT(i);
    }

    // Chi bat popup khi CANH BAO MOI xuat hien.
// Sau khi user xac nhan, canh bao van active neu chua het vuot nguong.
    if ((alarms & (uint8_t)~m->alarm_mask) != 0) {
        m->popup_alarm_index = high_priority_alarm((uint8_t)(alarms & ~m->alarm_mask));
        m->popup_visible = 1;
    }
    if (alarms == 0) m->popup_visible = 0;
    m->alarm_mask = alarms;
    m->dirty = 1;
}

void Menu_Init(Menu *m, Sh1106 *display, const MenuHooks *hooks)
{
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->display = display;
    if (hooks) m->hooks = *hooks;
    m->page = MENU_PAGE_HOME;   // Bat nguon -> DASHBOARD
    m->threshold[MENU_LIMIT_TEMP] = 350;   // 35.0 C
    m->threshold[MENU_LIMIT_HUM] = 800;    // 80.0 %
    m->threshold[MENU_LIMIT_PRESS] = 10500;// 1050.0 hPa
    m->threshold[MENU_LIMIT_GAS] = 2000;   // 2000 ADC
    m->dirty = 1;
}

void Menu_SetSensor(Menu *m, MenuSensor id, int32_t value, uint8_t valid)
{
    // Mau khong doi thi bo qua; mau moi se tinh lai canh bao va danh dau can ve.
    uint8_t before;
    if (!m || (unsigned)id >= MENU_SENSOR_COUNT) return;
    before = (m->valid_mask & BIT(id)) ? 1 : 0;
    valid = valid ? 1 : 0;
    if (m->sensor[id] == value && before == valid) return;
    m->sensor[id] = value;
    if (valid) m->valid_mask |= BIT(id);
    else m->valid_mask &= (uint8_t)~BIT(id);
    recompute_alarms(m);
}

void Menu_SetRTC(Menu *m, const MenuDateTime *date_time)
{
    if (!m) return;
    if (!valid_rtc(date_time)) {
        if (m->rtc_valid) { m->rtc_valid = 0; m->dirty = 1; }
        return;
    }
    if (m->rtc_valid && m->rtc.year == date_time->year && m->rtc.month == date_time->month &&
        m->rtc.day == date_time->day && m->rtc.hour == date_time->hour &&
        m->rtc.minute == date_time->minute && m->rtc.second == date_time->second)
        return;
    m->rtc = *date_time;
    m->rtc_valid = 1;
    m->dirty = 1;
}

void Menu_SetThreshold(Menu *m, MenuLimit id, int32_t value)
{
    if (!m || (unsigned)id >= MENU_LIMIT_COUNT) return;
    if (value < limit_min[id]) value = limit_min[id];
    if (value > limit_max[id]) value = limit_max[id];
    if (m->threshold[id] == value) return;
    m->threshold[id] = value;
    recompute_alarms(m);
}

int32_t Menu_GetThreshold(const Menu *m, MenuLimit id)
{
    return m && (unsigned)id < MENU_LIMIT_COUNT ? m->threshold[id] : 0;
}

uint8_t Menu_GetAlarmMask(const Menu *m)
{
    return m ? m->alarm_mask : 0;
}

static uint8_t wrap_up(uint8_t old, uint8_t count)
{
    return old == 0 ? (uint8_t)(count - 1) : (uint8_t)(old - 1);
}

static uint8_t wrap_down(uint8_t old, uint8_t count)
{
    return (uint8_t)((old + 1) % count);
}

static void adjust_threshold(Menu *m, int8_t sign)
{
    const MenuLimit k = m->editing_limit;
    int32_t v = m->edit_limit_value + sign * limit_step[k];
    if (v > limit_max[k]) v = limit_min[k];
    if (v < limit_min[k]) v = limit_max[k];
    m->edit_limit_value = v;
}

static void adjust_rtc_field(Menu *m, int8_t sign)
{
    // gia tri quay vong khi cham min/max.
    // Doi thang/nam thi gioi han lai ngay de tranh ngay 31 trong thang 2.
    MenuDateTime *d = &m->edit_rtc;
    uint16_t value = 0, min = 0, max = 59;
    switch (m->editing_rtc_field) {
        case 0: value = d->day; min = 1; max = days_in_month(d->year, d->month); break;
        case 1: value = d->month; min = 1; max = 12; break;
        case 2: value = d->year; min = 2000; max = 2099; break;
        case 3: value = d->hour; max = 23; break;
        case 4: value = d->minute; break;
        default: value = d->second; break;
    }
    value = sign > 0 ? (value == max ? min : (uint16_t)(value + 1))
                     : (value == min ? max : (uint16_t)(value - 1));
    switch (m->editing_rtc_field) {
        case 0: d->day = (uint8_t)value; break;
        case 1: d->month = (uint8_t)value; break;
        case 2: d->year = value; break;
        case 3: d->hour = (uint8_t)value; break;
        case 4: d->minute = (uint8_t)value; break;
        default: d->second = (uint8_t)value; break;
    }
    if (d->day > days_in_month(d->year, d->month))
        d->day = days_in_month(d->year, d->month);
}

void Menu_HandleButton(Menu *m, ButtonEvent event)
{
    // May trang thai: m->page quyet dinh UP/DOWN/SELECT dang lam viec gi.
    // edit_limit_value va edit_rtc la ban nhap, chi ap dung khi chon SAVE.
    if (!m || event == BUTTON_NONE) return;

    if (m->popup_visible) {
        // Popup canh bao: SELECT xac nhan (khong xoa tinh trang active).
        if (event == BUTTON_SELECT) m->popup_visible = 0;
        m->dirty = 1;
        return;
    }

    switch (m->page) {
        case MENU_PAGE_HOME:
            if (event == BUTTON_SELECT) m->page = MENU_PAGE_MAIN;
            break;

        case MENU_PAGE_MAIN:
            if (event == BUTTON_UP) m->selected_main = wrap_up(m->selected_main, 4);
            if (event == BUTTON_DOWN) m->selected_main = wrap_down(m->selected_main, 4);
            if (event == BUTTON_SELECT) {
                switch (m->selected_main) {
                    case 0: m->page = MENU_PAGE_LIMIT_LIST; break;
                    case 1:
                        m->edit_rtc = m->rtc_valid ? m->rtc :
                                      (MenuDateTime){2026, 1, 1, 0, 0, 0};
                        m->rtc_save_failed = 0;
                        m->selected_rtc = 0;
                        m->page = MENU_PAGE_RTC_LIST;
                        break;
                    case 2: m->page = MENU_PAGE_ALERTS; break;
                    default: m->page = MENU_PAGE_HOME; break; // BACK TO HOME
                }
            }
            break;

        case MENU_PAGE_ALERTS:
            // Man canh bao chi co 1 muc BACK, chon la ve main.
            if (event == BUTTON_SELECT) m->page = MENU_PAGE_MAIN;
            break;

        case MENU_PAGE_LIMIT_LIST:
            if (event == BUTTON_UP) m->selected_limit = wrap_up(m->selected_limit, 5);
            if (event == BUTTON_DOWN) m->selected_limit = wrap_down(m->selected_limit, 5);
            if (event == BUTTON_SELECT) {
                if (m->selected_limit == 4) m->page = MENU_PAGE_MAIN;
                else {
                    m->threshold_save_failed = 0;
                    m->editing_limit = (MenuLimit)m->selected_limit;
                    m->edit_limit_value = m->threshold[m->editing_limit];
                    m->page = MENU_PAGE_LIMIT_CHANGE;
                }
            }
            break;

        case MENU_PAGE_LIMIT_CHANGE:
            if (event == BUTTON_UP) adjust_threshold(m, 1);
            if (event == BUTTON_DOWN) adjust_threshold(m, -1);
            if (event == BUTTON_SELECT) {
                m->selected_action = 0;
                m->page = MENU_PAGE_LIMIT_CONFIRM;
            }
            break;

        case MENU_PAGE_LIMIT_CONFIRM:
            if (event == BUTTON_UP || event == BUTTON_DOWN)
                m->selected_action ^= 1; // > SAVE / BACK (CANCEL)
            if (event == BUTTON_SELECT) {
                if (m->selected_action == 0) {
                    // Ghi Flash thanh cong moi doi nguong trong RAM.
                    if (!m->hooks.threshold_saved ||
                        m->hooks.threshold_saved(m->editing_limit,
                            m->edit_limit_value, m->hooks.user) != 0) {
                        m->threshold_save_failed = 1;
                        break; // O lai de thu SAVE lai hoac CANCEL.
                    }
                    m->threshold_save_failed = 0;
                    Menu_SetThreshold(m, m->editing_limit, m->edit_limit_value);
                }
                m->page = MENU_PAGE_LIMIT_LIST;
            }
            break;

        case MENU_PAGE_RTC_LIST:
            if (event == BUTTON_UP) m->selected_rtc = wrap_up(m->selected_rtc, 8);
            if (event == BUTTON_DOWN) m->selected_rtc = wrap_down(m->selected_rtc, 8);
            if (event == BUTTON_SELECT) {
                if (m->selected_rtc < 6) {
                    m->editing_rtc_field = m->selected_rtc;
                    m->page = MENU_PAGE_RTC_CHANGE;
                } else if (m->selected_rtc == 6) { // SAVE
                    if (valid_rtc(&m->edit_rtc) && m->hooks.rtc_write &&
                        m->hooks.rtc_write(&m->edit_rtc, m->hooks.user) == 0) {
                        m->rtc = m->edit_rtc;
                        m->rtc_valid = 1;
                        m->rtc_save_failed = 0;
                        m->page = MENU_PAGE_MAIN;
                    } else m->rtc_save_failed = 1;
                } else {
                    m->page = MENU_PAGE_MAIN; // BACK: khong luu RTC
                }
            }
            break;

        case MENU_PAGE_RTC_CHANGE:
            if (event == BUTTON_UP) adjust_rtc_field(m, 1);
            if (event == BUTTON_DOWN) adjust_rtc_field(m, -1);
            if (event == BUTTON_SELECT) m->page = MENU_PAGE_RTC_LIST;
            break;

        default: m->page = MENU_PAGE_HOME; break;
    }
    m->dirty = 1;
}

// Danh sach da chon: auto scroll neu nhieu hon 6 muc.
static void draw_list(Menu *m, const char *title,
                      const char *const *items, uint8_t count, uint8_t selected)
{
    uint8_t start = selected >= LIST_VISIBLE ? (uint8_t)(selected - LIST_VISIBLE + 1) : 0;
    uint8_t i;
    char msg[24];
    line(m, 0, title);
    for (i = start; i < count && i < start + LIST_VISIBLE; ++i) {
        (void)snprintf(msg, sizeof(msg), "%c%s", i == selected ? '>' : ' ', items[i]);
        line(m, (uint8_t)(1 + i - start), msg);
    }
    line(m, 7, "UP/DN  SEL: CHOOSE");
}

static void draw_home(Menu *m)
{
    char linebuf[30], a[18], b[18];
    if (m->rtc_valid)
        (void)snprintf(linebuf, sizeof(linebuf), "%02u/%02u/%04u %02u:%02u:%02u",
                       (unsigned)m->rtc.day, (unsigned)m->rtc.month,
                       (unsigned)m->rtc.year, (unsigned)m->rtc.hour,
                       (unsigned)m->rtc.minute, (unsigned)m->rtc.second);
    else (void)snprintf(linebuf, sizeof(linebuf), "--/--/---- --:--:--");
    line(m, 0, linebuf);

    fmt_sensor(m, MENU_AHT_TEMP, a, sizeof(a));
    fmt_sensor(m, MENU_AHT_HUM, b, sizeof(b));
    (void)snprintf(linebuf, sizeof(linebuf), "AHT:%.9sC H:%.9s%%", a, b);
    line(m, 1, linebuf);
    fmt_sensor(m, MENU_BMP_TEMP, a, sizeof(a));
    (void)snprintf(linebuf, sizeof(linebuf), "BMP TEMP:%sC", a);
    line(m, 2, linebuf);
    fmt_sensor(m, MENU_BMP_PRESS, a, sizeof(a));
    (void)snprintf(linebuf, sizeof(linebuf), "PRESS:%shPa", a);
    line(m, 3, linebuf);
    fmt_sensor(m, MENU_MQ5_ADC, a, sizeof(a));
    (void)snprintf(linebuf, sizeof(linebuf), "MQ5 ADC:%s", a);
    line(m, 4, linebuf);

    if (m->alarm_mask)
        (void)snprintf(linebuf, sizeof(linebuf), "ALERT: %u ACTIVE", (unsigned)count_bits(m->alarm_mask));
    else if (m->valid_mask != SENSOR_ALL_VALID)
        (void)snprintf(linebuf, sizeof(linebuf), "STATUS: WAIT SENSOR");
    else (void)snprintf(linebuf, sizeof(linebuf), "STATUS: NORMAL");
    line(m, 5, linebuf);
    line(m, 6, "--------------------");
    line(m, 7, "SELECT: MENU");
}

static void draw_limit_change(Menu *m)
{
    char val[24], msg[30];
    line(m, 0, "CHANGE THRESHOLD");
    line(m, 2, limit_names[m->editing_limit]);
    fmt_limit(m->editing_limit, m->edit_limit_value, val, sizeof(val));
    (void)snprintf(msg, sizeof(msg), "VALUE: %.18s", val);
    line(m, 4, msg);
    line(m, 6, "UP/DN: CHANGE");
    line(m, 7, "SELECT: OPTIONS");
}

static void draw_limit_confirm(Menu *m)
{
    char msg[27], val[18];
    line(m, 0, "CONFIRM THRESHOLD");
    if (m->threshold_save_failed) line(m, 6, "FLASH SAVE FAILED!");
    fmt_limit(m->editing_limit, m->edit_limit_value, val, sizeof(val));
    (void)snprintf(msg, sizeof(msg), "NEW: %s", val);
    line(m, 2, msg);
    line(m, 4, m->selected_action == 0 ? "> SAVE" : "  SAVE");
    line(m, 5, m->selected_action == 1 ? "> BACK (CANCEL)" : "  BACK (CANCEL)");
    line(m, 7, "UP/DN  SEL: CHOOSE");
}

static void draw_rtc_list(Menu *m)
{
    char items[8][24];
    const char *ptrs[8];
    uint8_t i;
    (void)snprintf(items[0], sizeof(items[0]), "DAY:    %02u", (unsigned)m->edit_rtc.day);
    (void)snprintf(items[1], sizeof(items[1]), "MONTH:  %02u", (unsigned)m->edit_rtc.month);
    (void)snprintf(items[2], sizeof(items[2]), "YEAR:   %04u", (unsigned)m->edit_rtc.year);
    (void)snprintf(items[3], sizeof(items[3]), "HOUR:   %02u", (unsigned)m->edit_rtc.hour);
    (void)snprintf(items[4], sizeof(items[4]), "MINUTE: %02u", (unsigned)m->edit_rtc.minute);
    (void)snprintf(items[5], sizeof(items[5]), "SECOND: %02u", (unsigned)m->edit_rtc.second);
    (void)snprintf(items[6], sizeof(items[6]), "SAVE");
    (void)snprintf(items[7], sizeof(items[7]), "BACK (CANCEL)");
    for (i = 0; i < 8; ++i) ptrs[i] = items[i];
    draw_list(m, "SET RTC DATE/TIME", ptrs, 8, m->selected_rtc);
    if (m->rtc_save_failed) line(m, 7, "RTC SAVE FAILED!");
}

static void draw_rtc_change(Menu *m)
{
    char msg[26];
    MenuDateTime *d = &m->edit_rtc;
    line(m, 0, "CHANGE RTC FIELD");
    line(m, 2, rtc_names[m->editing_rtc_field]);
    (void)snprintf(msg, sizeof(msg), "DATE: %02u/%02u/%04u", (unsigned)d->day,
                   (unsigned)d->month, (unsigned)d->year);
    line(m, 3, msg);
    (void)snprintf(msg, sizeof(msg), "TIME: %02u:%02u:%02u", (unsigned)d->hour,
                   (unsigned)d->minute, (unsigned)d->second);
    line(m, 4, msg);
    line(m, 6, "UP/DN: CHANGE");
    line(m, 7, "SELECT: DONE");
}

static void draw_alerts(Menu *m)
{
    uint8_t i, row = 1;
    char msg[25];
    (void)snprintf(msg, sizeof(msg), "ACTIVE ALERTS: %u", (unsigned)count_bits(m->alarm_mask));
    line(m, 0, msg);
    if (!m->alarm_mask) line(m, 2, "NO ACTIVE ALERTS");
    for (i = 0; i < MENU_LIMIT_COUNT; ++i) {
        if (m->alarm_mask & BIT(i)) {
            (void)snprintf(msg, sizeof(msg), "! %s HIGH", limit_names[i]);
            line(m, row++, msg);
        }
    }
    line(m, 7, "> BACK");
}

static void draw_popup(Menu *m)
{
    char msg[30], val[20];
    const MenuLimit k = (MenuLimit)m->popup_alarm_index;
    line(m, 0, "!!! WARNING !!!");
    (void)snprintf(msg, sizeof(msg), "%s HIGH", limit_names[k]);
    line(m, 2, msg);
    fmt_sensor(m, limit_source[k], val, sizeof(val));
    (void)snprintf(msg, sizeof(msg), "VALUE: %.18s", val);
    line(m, 3, msg);
    fmt_limit(k, m->threshold[k], val, sizeof(val));
    (void)snprintf(msg, sizeof(msg), "LIMIT: %s", val);
    line(m, 4, msg);
    (void)snprintf(msg, sizeof(msg), "ACTIVE ALARMS: %u", (unsigned)count_bits(m->alarm_mask));
    line(m, 5, msg);
    line(m, 7, "SELECT: ACKNOWLEDGE");
}

int Menu_Render(Menu *m)
{
    // dirty = 1 nghia la noi dung da doi. Ve vao RAM truoc, roi day len OLED.
    // Neu I2C loi thi giu dirty de luot Poll sau thu gui lai.
    if (!m || !m->display) return -1;
    if (!m->dirty) return 0;
    SH1106_Clear(m->display);
    if (m->popup_visible) draw_popup(m);
    else switch (m->page) {
        case MENU_PAGE_HOME: draw_home(m); break;
        case MENU_PAGE_MAIN: draw_list(m, "MAIN MENU", main_items, 4, m->selected_main); break;
        case MENU_PAGE_LIMIT_LIST:
            draw_list(m, "SET THRESHOLD", threshold_items, 5, m->selected_limit);
            break;
        case MENU_PAGE_LIMIT_CHANGE: draw_limit_change(m); break;
        case MENU_PAGE_LIMIT_CONFIRM: draw_limit_confirm(m); break;
        case MENU_PAGE_RTC_LIST: draw_rtc_list(m); break;
        case MENU_PAGE_RTC_CHANGE: draw_rtc_change(m); break;
        case MENU_PAGE_ALERTS: draw_alerts(m); break;
        default: draw_home(m); break;
    }
    if (SH1106_Display(m->display) != 0) return -1;
    m->dirty = 0;
    return 1;
}
