#include "task_ui.h"
#include "task.h"
#include "app_ipc.h"
#include "app_log.h"
#include "board_i2c.h"
#include "menu.h"
#include "rtc.h"
#include "threshold_store.h"

typedef enum {
    DISPLAY_OFFLINE, DISPLAY_FIRST_FRAME, DISPLAY_WAIT_POWER, DISPLAY_ONLINE
} DisplayState;

static Menu menu;
static Sh1106 oled;
static DisplayState display_state;
static TickType_t display_state_since;
static TickType_t last_frame;
static uint8_t next_chunk = SH1106_CHUNK_COUNT;

// Caller giu mutex cho ca lenh dinh vi va du lieu cua mot doan OLED.
static int oled_write(uint8_t address, const uint8_t *data, uint16_t size, void *user)
{
    (void)user;
    return Board_I2CWrite(address, data, size) ? 0 : -1;
}

static int save_threshold(MenuLimit id, int32_t value, void *user)
{
    (void)user;
    int32_t thresholds[MENU_LIMIT_COUNT];
    App_GetThresholds(thresholds);
    thresholds[id] = value;
#ifndef APP_MOCK_UI
    if (ThresholdStore_Save(thresholds) != 0) {
        App_LogMessage("SET THRESHOLDS: flash save failed\r\n");
        return -1;
    }
#endif
    App_SetThresholds(thresholds);
    App_LogMessage("SET THRESHOLDS: saved\r\n");
    return 0;
}

static int write_rtc(const MenuDateTime *value, void *user)
{
    (void)user;
#ifndef APP_MOCK_UI
    static const uint8_t offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    const unsigned year = value->year - (value->month < 3U ? 1U : 0U);
    const uint8_t weekday = (uint8_t)((year + year / 4U - year / 100U + year / 400U +
                                      offsets[value->month - 1U] + value->day) % 7U + 1U);
    const DATE date = {value->day, value->month, value->year, weekday};
    const TIME time = {value->hour, value->minute, value->second};
    if (!App_I2CTake(0))
        return -1;
    const bool saved = setRTC(&date, &time);
    App_I2CGive();
    if (!saved) {
        App_LogMessage("SET RTC: write failed\r\n");
        return -1;
    }
#endif
    Menu_SetRTC(&menu, value);
    App_LogMessage("SET RTC: saved\r\n");
    return 0;
}

static bool read_update(void)
{
    AlarmData_t update;
    if (!App_IPCReceiveAlarm(&update))
        return false;
    const bool alarm_changed = menu.alarm_mask != update.alarm_mask;
    for (unsigned i = 0; i < MENU_SENSOR_COUNT; ++i)
        Menu_SetSensor(&menu, (MenuSensor)i, update.data.sensor[i], update.data.valid_mask & (1U << i));
    Menu_SetRTC(&menu, update.data.rtc_valid ? &update.data.rtc : NULL);
    Menu_SetAlarmMask(&menu, update.alarm_mask);
    return alarm_changed;
}

static bool poll_button(ButtonManager *buttons, TickType_t now)
{
    const ButtonEvent event = Button_Poll(buttons, now * portTICK_PERIOD_MS);
    switch (event) {
        case BUTTON_UP:     App_LogMessage("BUTTON: UP\r\n"); break;
        case BUTTON_DOWN:   App_LogMessage("BUTTON: DOWN\r\n"); break;
        case BUTTON_SELECT: App_LogMessage("BUTTON: SELECT\r\n"); break;
        default: return false;
    }
    if (display_state != DISPLAY_ONLINE) {
        App_LogMessage("BUTTON: OLED unavailable\r\n");
        return false;
    }
    Menu_HandleButton(&menu, event);
    return true;
}

static void run_display(TickType_t now, bool redraw)
{
    if (display_state == DISPLAY_OFFLINE) {
        if (now - display_state_since < pdMS_TO_TICKS(1000) || !App_I2CTake(0))
            return;
        display_state_since = now;
        const bool configured = SH1106_Configure(&oled, oled_write, NULL, SH1106_ADDR7) == 0 ||
                                SH1106_Configure(&oled, oled_write, NULL, 0x3D) == 0;
        App_I2CGive();
        if (configured) {
            display_state = DISPLAY_FIRST_FRAME;
            menu.dirty = 1;
            Menu_Draw(&menu);
            next_chunk = 0;
        }
        return;
    }

    if (display_state == DISPLAY_WAIT_POWER) {
        if (now - display_state_since < pdMS_TO_TICKS(100) || !App_I2CTake(0))
            return;
        const bool powered = SH1106_PowerOn(&oled) == 0;
        App_I2CGive();
        display_state = powered ? DISPLAY_ONLINE : DISPLAY_OFFLINE;
        display_state_since = now;
        last_frame = now - pdMS_TO_TICKS(400);
        return;
    }

    if (display_state == DISPLAY_ONLINE &&
        (redraw || (next_chunk == SH1106_CHUNK_COUNT && now - last_frame >= pdMS_TO_TICKS(400))) &&
        Menu_Draw(&menu)) {
        last_frame = now;
        next_chunk = 0;
    }

    if (next_chunk == SH1106_CHUNK_COUNT || !App_I2CTake(0))
        return;
    const bool sent = SH1106_DisplayChunk(&oled, next_chunk) == 0;
    App_I2CGive();
    if (!sent) {
        display_state = DISPLAY_OFFLINE;
        display_state_since = xTaskGetTickCount();
        return;
    }
    if (++next_chunk == SH1106_CHUNK_COUNT && display_state == DISPLAY_FIRST_FRAME) {
        display_state = DISPLAY_WAIT_POWER;
        display_state_since = xTaskGetTickCount();
    }
}

#ifdef APP_MOCK_UI
static void publish_mock_update(TickType_t now)
{
    static TickType_t previous;
    static unsigned seconds;
    if (seconds != 0 && now - previous < pdMS_TO_TICKS(1000))
        return;
    previous = now;
    ++seconds;
    AlarmData_t update = {
        .data = {
            .sensor = {255, 620, 248, 10132, 400},
            .valid_mask = (1U << MENU_SENSOR_COUNT) - 1U,
            .rtc = {2026, 1, 1, 12, (seconds / 60) % 60, seconds % 60},
            .rtc_valid = true,
        },
    };
    if (seconds % 10 >= 5) {
        update.data.sensor[MENU_MQ5_ADC] = 3000;
        update.alarm_mask = 1U << MENU_LIMIT_GAS;
    }
    App_IPCPublishAlarm(&update);
}
#endif

static void prvUITask(void *parameters)
{
    (void)parameters;
    const TickType_t button_period = pdMS_TO_TICKS(10);
    ButtonManager buttons;
    Button_Init(&buttons, 25U);
    const MenuHooks hooks = {save_threshold, write_rtc, NULL};
    Menu_Init(&menu, &oled, &hooks);
    int32_t thresholds[MENU_LIMIT_COUNT];
    App_GetThresholds(thresholds);
    for (unsigned i = 0; i < MENU_LIMIT_COUNT; ++i)
        Menu_SetThreshold(&menu, (MenuLimit)i, thresholds[i]);

    TickType_t last_wake = xTaskGetTickCount();
    display_state_since = last_wake - pdMS_TO_TICKS(1000);
    for (;;) {
        const TickType_t now = xTaskGetTickCount();
#ifdef APP_MOCK_UI
        publish_mock_update(now);
#endif
        const bool alarm_changed = read_update();
        const bool button_pressed = poll_button(&buttons, now);
        run_display(now, alarm_changed || button_pressed);

        const TickType_t elapsed = xTaskGetTickCount() - last_wake;
        last_wake += (elapsed / button_period) * button_period;
        vTaskDelayUntil(&last_wake, button_period);
    }
}

BaseType_t task_ui_create(void)
{
    return xTaskCreate(prvUITask, "UI_TASK", 640, NULL, tskIDLE_PRIORITY + 1, NULL);
}
