#include "rtc.h"
extern "C" {
#include "i2c_rtos.h"
}
#include <stddef.h>

// Luu ma loi RTC gan nhat
static uint8_t last_error;

// Kiem tra tung chu so BCD
static uint8_t bcd_valid(uint8_t value)
{
    return (value & 0x0FU) <= 9U && (value >> 4) <= 9U;
}

// Chuyen BCD sang so thap phan
static uint8_t bcd_decode(uint8_t value)
{
    return (uint8_t)((value >> 4) * 10U + (value & 0x0FU));
}

// Chuyen so thap phan sang BCD
static uint8_t bcd_encode(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4) | (value % 10U));
}

// Kiem tra ngay gio hop le
static uint8_t time_valid(const RTC_Time *time)
{
    static const uint8_t days_in_month[] =
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Kiem tra nam, thang, thu va gio
    if (time->year < 2000U || time->year > 2099U ||
        time->month < 1U || time->month > 12U ||
        time->weekday < 1U || time->weekday > 7U ||
        time->second > 59U || time->minute > 59U || time->hour > 23U)
        return 0;

    // Lay so ngay cua thang
    uint8_t days = days_in_month[time->month - 1U];

    // Xu ly thang 2 trong nam nhuan
    if (time->month == 2U && time->year % 4U == 0U)
        ++days;

    return time->day >= 1U && time->day <= days;
}

// Doc thoi gian tu DS1307
uint8_t RTC_Read(RTC_Time *time)
{
    uint8_t raw[7];
    RTC_Time sample;

    // Kiem tra con tro nhan du lieu
    if (time == NULL)
    {
        last_error = RTC_ERROR_ARGUMENT;
        return 0;
    }

    // Doc 7 thanh ghi lien tiep tu dia chi 0x00
    last_error = I2C_RTOS_ReadRegBytes(RTC_I2C_ADDRESS, 0, raw, sizeof(raw));
    if (last_error != RTC_ERROR_NONE)
        return 0;

    // CH = 1: Bo dao dong dang dung
    if (raw[0] & 0x80U)
    {
        last_error = RTC_ERROR_CLOCK_HALTED;
        return 0;
    }

    // Kiem tra cac bit du tru va ma BCD
    last_error = RTC_ERROR_INVALID_TIME;
    if ((raw[1] & 0x80U) || (raw[2] & 0x80U) || (raw[3] & 0xF8U) ||
        (raw[4] & 0xC0U) || (raw[5] & 0xE0U) ||
        !bcd_valid(raw[0]) || !bcd_valid(raw[1]) ||
        !bcd_valid(raw[4]) || !bcd_valid(raw[5]) || !bcd_valid(raw[6]))
        return 0;

    // Giai ma giay va phut
    sample.second = bcd_decode(raw[0]);
    sample.minute = bcd_decode(raw[1]);

    // Bit 6 bang 1: Che do 12 gio
    if (raw[2] & 0x40U)
    {
        uint8_t hour = raw[2] & 0x1FU;
        if (!bcd_valid(hour))
            return 0;
        hour = bcd_decode(hour);
        if (hour < 1U || hour > 12U)
            return 0;
        // Doi sang gio 24h, bit 5 bang 1 la PM
        sample.hour = (uint8_t)((hour % 12U) + ((raw[2] & 0x20U) ? 12U : 0U));
    }

    // Che do 24 gio
    else
    {
        if (!bcd_valid(raw[2]))
            return 0;
        sample.hour = bcd_decode(raw[2]);
    }

    // Giai ma thu, ngay, thang va nam
    sample.weekday = raw[3];
    sample.day = bcd_decode(raw[4]);
    sample.month = bcd_decode(raw[5]);
    sample.year = (uint16_t)(2000U + bcd_decode(raw[6]));

    // Kiem tra ngay gio sau khi giai ma
    if (!time_valid(&sample))
        return 0;

    // Chi cap nhat ket qua khi du lieu hop le
    *time = sample;
    last_error = RTC_ERROR_NONE;
    return 1;
}

// Dat thoi gian cho DS1307
uint8_t RTC_Set(const RTC_Time *time)
{
    // Kiem tra con tro va ngay gio can ghi
    if (time == NULL)
    {
        last_error = RTC_ERROR_ARGUMENT;
        return 0;
    }
    if (!time_valid(time))
    {
        last_error = RTC_ERROR_INVALID_TIME;
        return 0;
    }

    // Byte 0: Dia chi thanh ghi bat dau
    // Byte 1-7: Giay, phut, gio, thu, ngay, thang, nam
    // Dung gio 24h va xoa CH de dong ho chay
    const uint8_t raw[] = {
        0, bcd_encode(time->second), bcd_encode(time->minute),
        bcd_encode(time->hour), time->weekday, bcd_encode(time->day),
        bcd_encode(time->month), bcd_encode((uint8_t)(time->year - 2000U))
    };
    // Gui toan bo du lieu trong mot giao dich
    last_error = I2C_RTOS_Write(RTC_I2C_ADDRESS, raw, sizeof(raw));
    return last_error == RTC_ERROR_NONE;
}

// Doc ma loi RTC gan nhat
uint8_t RTC_GetLastError(void)
{
    return last_error;
}
