#include "menu.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// valid_mask danh dau cam bien hop le; alarm_mask danh dau nguong dang canh bao.
#define BIT(n) ((uint8_t)(1 << (n)))
#define SENSOR_ALL_VALID ((uint8_t)((1 << MENU_SENSOR_COUNT) - 1))
#define LIST_VISIBLE 6

// Gioi han va buoc dieu chinh cua tung nguong trong menu.
// Nhiet do, do am va ap suat luu theo don vi nhan 10; gas dung gia tri ADC 12 bit.
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

// Ve chu tu cot 0; moi dong chiem 8 pixel theo chieu doc.
static void line(Menu *m, uint8_t row, const char *str)
{
    SH1106_DrawText(m->display, 0, (uint8_t)(row * 8), str);
}

// Dem so bit 1 de hien thi so canh bao dang hoat dong.
static uint8_t count_bits(uint8_t v)
{
    uint8_t n = 0;
    while (v) { n += (uint8_t)(v & 1); v >>= 1; }
    return n;
}

// Tra so ngay cua thang, tinh ca nam nhuan; thang sai tra ve 31.
static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t leap;
    if (month < 1 || month > 12) return 31;
    leap = (uint8_t)((year % 4 == 0) &&
                     ((year % 100 != 0) || (year % 400 == 0)));
    return (month == 2 && leap) ? 29 : days[month - 1];
}

// Kiem tra ngay gio hop le trong khoang nam 2000 den 2099.
static uint8_t valid_rtc(const MenuDateTime *d)
{
    return d && d->year >= 2000 && d->year <= 2099 &&
        d->month >= 1 && d->month <= 12 &&
        d->day >= 1 && d->day <= days_in_month(d->year, d->month) &&
        d->hour <= 23 && d->minute <= 59 && d->second <= 59;
}

// Dinh dang so nguyen nhan 10 thanh chuoi co mot chu so thap phan.
// Vi du: 253 thanh "25.3", -32 thanh "-3.2"; khong can phep tinh float.
static void fmt_x10(char *buf, size_t len, int32_t scaled)
{
    uint32_t abs_value = (uint32_t)(scaled < 0 ? -(int64_t)scaled : scaled);
    (void)snprintf(buf, len, "%s%lu.%lu", scaled < 0 ? "-" : "",
                   (unsigned long)(abs_value / 10),
                   (unsigned long)(abs_value % 10));
}

// Hien "--" neu mau loi; MQ5 hien ADC, cac cam bien khac hien so thap phan.
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

// Dinh dang nguong kem don vi ADC, %, hPa hoac C.
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

// Chon mot canh bao theo thu tu uu tien de hien popup.
static uint8_t high_priority_alarm(uint8_t alarms)
{
    // Thu tu hien popup: gas, nhiet do, do am, sau cung la ap suat.
    static const uint8_t order[] = {
        MENU_LIMIT_GAS, MENU_LIMIT_TEMP, MENU_LIMIT_HUM, MENU_LIMIT_PRESS
    };
    uint8_t i;
    for (i = 0; i < MENU_LIMIT_COUNT; ++i)
        if (alarms & BIT(order[i])) return order[i];
    return MENU_LIMIT_TEMP;
}

// Tinh lai cac bit canh bao va yeu cau cap nhat man hinh.
static void recompute_alarms(Menu *m)
{
    // Chi bat canh bao cho mau hop le co gia tri lon hon hoac bang nguong.
    uint8_t i, alarms = 0;
    for (i = 0; i < MENU_LIMIT_COUNT; ++i) {
        MenuSensor s = limit_source[i];
        if ((m->valid_mask & BIT(s)) && m->sensor[s] >= m->threshold[i])
            alarms |= BIT(i);
    }

    // Chi mo popup khi co bit canh bao moi chuyen tu 0 sang 1.
    // Xac nhan chi dong popup; canh bao van con neu mau van cham hoac vuot nguong.
    if ((alarms & (uint8_t)~m->alarm_mask) != 0) {
        m->popup_alarm_index = high_priority_alarm((uint8_t)(alarms & ~m->alarm_mask));
        m->popup_visible = 1;
    }
    if (alarms == 0) m->popup_visible = 0;
    m->alarm_mask = alarms;
    m->dirty = 1;
}

// Khoi tao trang chu, callback va cac nguong mac dinh.
void Menu_Init(Menu *m, Sh1106 *display, const MenuHooks *hooks)
{
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->display = display;
    if (hooks) m->hooks = *hooks;
    m->page = MENU_PAGE_HOME;   // Trang hien thi ban dau sau khi khoi tao.
    m->threshold[MENU_LIMIT_TEMP] = 350;   // 35.0 C
    m->threshold[MENU_LIMIT_HUM] = 800;    // 80.0 %
    m->threshold[MENU_LIMIT_PRESS] = 10500;// 1050.0 hPa
    m->threshold[MENU_LIMIT_GAS] = 2000;   // 2000 ADC
    m->dirty = 1;
}

// Cap nhat mau cam bien, trang thai hop le va cac canh bao lien quan.
void Menu_SetSensor(Menu *m, MenuSensor id, int32_t value, uint8_t valid)
{
    // Bo qua neu ca gia tri va trang thai hop le deu khong doi.
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

// Cap nhat ngay gio; danh dau RTC khong hop le neu du lieu dau vao sai.
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

// Gioi han nguong vao mien cho phep, sau do tinh lai canh bao.
void Menu_SetThreshold(Menu *m, MenuLimit id, int32_t value)
{
    if (!m || (unsigned)id >= MENU_LIMIT_COUNT) return;
    if (value < limit_min[id]) value = limit_min[id];
    if (value > limit_max[id]) value = limit_max[id];
    if (m->threshold[id] == value) return;
    m->threshold[id] = value;
    recompute_alarms(m);
}

// Doc nguong hien tai; tra 0 neu menu hoac chi so khong hop le.
int32_t Menu_GetThreshold(const Menu *m, MenuLimit id)
{
    return m && (unsigned)id < MENU_LIMIT_COUNT ? m->threshold[id] : 0;
}

// Doc cac bit canh bao dang bat; tra 0 neu menu khong hop le.
uint8_t Menu_GetAlarmMask(const Menu *m)
{
    return m ? m->alarm_mask : 0;
}

// Di len mot muc; tu muc dau quay ve muc cuoi.
static uint8_t wrap_up(uint8_t old, uint8_t count)
{
    return old == 0 ? (uint8_t)(count - 1) : (uint8_t)(old - 1);
}

// Di xuong mot muc; tu muc cuoi quay ve muc dau.
static uint8_t wrap_down(uint8_t old, uint8_t count)
{
    return (uint8_t)((old + 1) % count);
}

// Chinh ban nhap nguong theo buoc cua cam bien va quay vong tai hai dau.
static void adjust_threshold(Menu *m, int8_t sign)
{
    const MenuLimit k = m->editing_limit;
    int32_t v = m->edit_limit_value + sign * limit_step[k];
    if (v > limit_max[k]) v = limit_min[k];
    if (v < limit_min[k]) v = limit_max[k];
    m->edit_limit_value = v;
}

// Chinh mot truong ngay gio trong ban nhap, chua gui yeu cau ghi RTC.
static void adjust_rtc_field(Menu *m, int8_t sign)
{
    // Tang/giam truong dang chon, quay vong khi vuot gioi han.
    // Sau khi doi thang hoac nam, ha ngay xuong neu vuot so ngay cua thang.
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

// Dieu huong menu, sua ban nhap va goi callback khi nguoi dung chon SAVE.
void Menu_HandleButton(Menu *m, ButtonEvent event)
{
    // Xu ly UP/DOWN/SELECT theo trang menu hien tai.
    // Gia tri dang sua nam trong ban nhap; chi cap nhat khi SAVE thanh cong.
    if (!m || event == BUTTON_NONE) return;

    if (m->popup_visible) {
        // Khi popup dang mo, chi SELECT dong popup; cac nut khac khong doi trang.
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
                    default: m->page = MENU_PAGE_HOME; break; // Quay lai trang chu.
                }
            }
            break;

        case MENU_PAGE_ALERTS:
            // SELECT tai trang canh bao de quay ve menu chinh.
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
                m->selected_action ^= 1; // Chuyen lua chon giua SAVE va BACK (CANCEL).
            if (event == BUTTON_SELECT) {
                if (m->selected_action == 0) {
                    // Chi doi nguong trong RAM khi callback luu nguong tra ve 0.
                    if (!m->hooks.threshold_saved ||
                        m->hooks.threshold_saved(m->editing_limit,
                            m->edit_limit_value, m->hooks.user) != 0) {
                        m->threshold_save_failed = 1;
                        break; // Giu trang xac nhan de luu lai hoac huy.
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
                    m->page = MENU_PAGE_MAIN; // Huy ban nhap ngay gio va quay ve menu chinh.
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

// Ve toi da 6 muc; cuon danh sach de muc dang chon luon hien thi.
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

// Ve ngay gio, cac mau cam bien va trang thai canh bao tren trang chu.
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

// Ve ten nguong dang sua, gia tri ban nhap va huong dan nut bam.
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

// Ve lua chon luu/huy nguong va thong bao neu lan luu truoc that bai.
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

// Tao danh sach 6 truong ngay gio cung hai lua chon SAVE va BACK.
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

// Ve ngay gio ban nhap va ten truong dang duoc chinh.
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

// Liet ke cac nguong dang canh bao va lua chon quay lai.
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

// Ve canh bao uu tien, gia tri cam bien va nguong tuong ung.
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

// Tra -1 khi loi, 0 khi khong can ve, 1 khi da gui noi dung moi len OLED.
int Menu_Render(Menu *m)
{
    // Chi ve khi dirty = 1; dung noi dung trong bo dem truoc khi gui len OLED.
    // Neu gui OLED that bai, giu dirty de lan goi Menu_Render sau thu lai.
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
