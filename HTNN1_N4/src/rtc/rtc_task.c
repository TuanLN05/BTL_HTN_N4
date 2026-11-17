#include "rtc_task.h"
#include "task.h"
#include "app_ipc.h"

#define RTC_TASK_STACK_WORDS 256U
#define RTC_TASK_PRIORITY    (tskIDLE_PRIORITY + 1U)
#define RTC_SAMPLE_MS        1000U

volatile RTC_Time rtc_time;
volatile uint8_t rtc_read_ok;
volatile uint8_t rtc_error;
volatile uint32_t rtc_read_count;

volatile RTC_Time rtc_set_time;
volatile uint8_t rtc_set_request;
volatile uint8_t rtc_set_ok;
volatile uint8_t rtc_set_error;

// Lay ban sao ngay gio cho task man hinh
uint8_t RTC_GetSnapshot(DATE *date, TIME *time)
{
    if (date == NULL || time == NULL)
        return 0;

    // Sao chep thoi gian va trang thai trong cung mot vung bao ve
    RTC_Time sample;
    uint8_t ready;
    taskENTER_CRITICAL();
    sample = rtc_time;
    ready = rtc_read_ok && rtc_read_count != 0;
    taskEXIT_CRITICAL();

    if (!ready)
        return 0;

    // Tach ngay va gio sau khi roi vung bao ve
    const DATE date_sample = {
        .day = sample.day,
        .month = sample.month,
        .year = sample.year,
        .weekday = sample.weekday
    };
    const TIME time_sample = {
        .hour = sample.hour,
        .minute = sample.minute,
        .second = sample.second
    };
    *date = date_sample;
    *time = time_sample;
    return 1;
}

// Giao dien doc RTC theo quy uoc cua nhom
void readRTC(DATE *date, TIME *time)
{
    (void)RTC_GetSnapshot(date, time);
}

// Menu khong gui thu; tinh theo ngay, voi 1 = Chu nhat.
static uint8_t rtc_weekday(const MenuDateTime *time)
{
    static const uint8_t month_days[] =
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (time->year < 2000U || time->year > 2099U ||
        time->month < 1U || time->month > 12U ||
        time->day < 1U || time->day > 31U)
        return 1U; // RTC_Set se tu choi ngay gio khong hop le.

    uint32_t days = time->day - 1U;
    for (uint16_t year = 2000U; year < time->year; ++year)
        days += (year % 4U == 0U) ? 366U : 365U;
    for (uint8_t month = 1U; month < time->month; ++month)
        days += month_days[month - 1U];
    if (time->month > 2U && time->year % 4U == 0U)
        ++days;
    // Ngay 01/01/2000 la Thu bay.
    return (uint8_t)((days + 6U) % 7U + 1U);
}

// Chi goi trong task RTC: nhan mot yeu cau SAVE va ghi xuong DS1307.
static void rtc_receive_menu_request(void)
{
    MenuDateTime menu_time;
    if (g_rtc_request_queue == NULL ||
        xQueueReceive(g_rtc_request_queue, &menu_time, 0) != pdPASS)
        return;

    const RTC_Time requested = {
        .year = menu_time.year,
        .month = menu_time.month,
        .day = menu_time.day,
        .weekday = rtc_weekday(&menu_time),
        .hour = menu_time.hour,
        .minute = menu_time.minute,
        .second = menu_time.second
    };
    uint8_t ok = RTC_Set(&requested);
    uint8_t error = RTC_GetLastError();
    taskENTER_CRITICAL();
    rtc_set_ok = ok;
    rtc_set_error = error;
    taskEXIT_CRITICAL();
}

// Doc va cap nhat thoi gian moi giay
static void rtc_task(void *argument)
{
    (void)argument;

    // Lay moc tick ban dau
    TickType_t last_wake = xTaskGetTickCount();

    for (;;)
    {
        RTC_Time requested;
        uint8_t set_pending;

        // Lay yeu cau dat gio, giu co den khi ghi RTC xong
        taskENTER_CRITICAL();
        set_pending = rtc_set_request;
        if (set_pending)
            requested = rtc_set_time;
        taskEXIT_CRITICAL();

        // Xu ly yeu cau dat gio
        if (set_pending)
        {
            uint8_t ok = RTC_Set(&requested);
            uint8_t error = RTC_GetLastError();

            // Luu ket qua va xoa yeu cau
            taskENTER_CRITICAL();
            rtc_set_ok = ok;
            rtc_set_error = error;
            rtc_set_request = 0;
            taskEXIT_CRITICAL();
        }

        rtc_receive_menu_request();

        // Doc thoi gian tu DS1307
        RTC_Time sample;
        uint8_t ok = RTC_Read(&sample);
        uint8_t error = RTC_GetLastError();
        // Cap nhat ket qua, giu mau cu neu doc loi
        taskENTER_CRITICAL();
        if (ok)
        {
            rtc_time = sample;
            ++rtc_read_count;
        }
        rtc_read_ok = ok;
        rtc_error = error;
        taskEXIT_CRITICAL();

        // Gui ngay gio doc thuc te tu chip ve task OLED.
        if (ok)
        {
            const MenuDateTime display_time = {
                .year = sample.year,
                .month = sample.month,
                .day = sample.day,
                .hour = sample.hour,
                .minute = sample.minute,
                .second = sample.second
            };
            (void)Display_PostRTC(&display_time);
        }

        // Cho den chu ky doc tiep theo
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(RTC_SAMPLE_MS));
    }
}

// Tao task RTC
BaseType_t RTC_TaskStart(void)
{
    return xTaskCreate(rtc_task, "RTC", RTC_TASK_STACK_WORDS, NULL,
                       RTC_TASK_PRIORITY, NULL);
}
