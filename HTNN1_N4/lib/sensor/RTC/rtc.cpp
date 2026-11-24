#include "rtc.h"

static uint8_t bcd_decode(uint8_t value)
{
    if ((value & 0x0FU) > 9U || value > 0x99U)
        return 0xFFU;
    return (uint8_t)((value >> 4) * 10U + (value & 0x0FU));
}

static uint8_t bcd_encode(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4) | (value % 10U));
}

static bool date_time_valid(const DATE *date, const TIME *time)
{
    static const uint8_t days_in_month[] =
        {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    if (date->year < 2000U || date->year > 2099U ||
        date->month < 1U || date->month > 12U ||
        date->weekday < 1U || date->weekday > 7U ||
        time->hour > 23U || time->minute > 59U || time->second > 59U)
        return false;

    uint8_t days = days_in_month[date->month - 1U];
    if (date->month == 2U && date->year % 4U == 0U)
        ++days;
    return date->day >= 1U && date->day <= days;
}

void rtc_init(void)
{
    uint8_t reg = 0U;
    uint8_t raw[7] = {0U};
    i2c1_writeBytes(rtc_addr, &reg, 1U);
    i2c1_readBytes(rtc_addr, raw, sizeof(raw));

    // Chi bat dao dong neu dang dung, giu nguyen ngay gio
    uint8_t seconds = raw[0] & 0x7FU;
    if ((raw[0] & 0x80U) && bcd_decode(seconds) <= 59U)
    {
        uint8_t command[] = {0U, seconds};
        i2c1_writeBytes(rtc_addr, command, sizeof(command));
    }
}

void readRTC(DATE *date, TIME *time)
{
    if (date == nullptr || time == nullptr)
        return;

    uint8_t reg = 0U;
    uint8_t raw[7] = {0U};
    i2c1_writeBytes(rtc_addr, &reg, 1U);
    i2c1_readBytes(rtc_addr, raw, sizeof(raw));
    if (raw[2] & 0x80U)
        return;

    DATE sample_date = {bcd_decode(raw[4]), bcd_decode(raw[5]),
                        (uint16_t)(2000U + bcd_decode(raw[6])), raw[3]};
    TIME sample_time = {bcd_decode(raw[2]), bcd_decode(raw[1]), bcd_decode(raw[0])};

    // Doi che do 12 gio sang 24 gio
    if (raw[2] & 0x40U)
    {
        uint8_t hour = bcd_decode(raw[2] & 0x1FU);
        if (hour < 1U || hour > 12U)
            return;
        sample_time.hour = (uint8_t)((hour % 12U) + ((raw[2] & 0x20U) ? 12U : 0U));
    }
    if (!date_time_valid(&sample_date, &sample_time))
        return;

    *date = sample_date;
    *time = sample_time;
}

void setRTC(const DATE *date, const TIME *time)
{
    if (date == nullptr || time == nullptr || !date_time_valid(date, time))
        return;

    uint8_t raw[] = {
        0U, bcd_encode(time->second), bcd_encode(time->minute),
        bcd_encode(time->hour), date->weekday, bcd_encode(date->day),
        bcd_encode(date->month), bcd_encode((uint8_t)(date->year - 2000U))
    };
    i2c1_writeBytes(rtc_addr, raw, sizeof(raw));
}
