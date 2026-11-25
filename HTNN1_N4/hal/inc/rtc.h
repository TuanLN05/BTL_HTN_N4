#ifndef RTC_H
#define RTC_H

#include <stdbool.h>
#include <stdint.h>

#define rtc_addr 0x68U

typedef struct
{
    uint8_t day, month;
    uint16_t year;    // 2000-2099
    uint8_t weekday;  // 1: Chu nhat, ..., 7: Thu bay
} DATE;

typedef struct
{
    uint8_t hour, minute, second; // Gio 24h
} TIME;

#ifdef __cplusplus
extern "C" {
#endif

// Goi sau i2c1_init, khong dat lai ngay gio
bool rtc_init(void);

// Giu dau ra cu neu con tro NULL hoac du lieu sai
bool readRTC(DATE *date, TIME *time);

bool setRTC(const DATE *date, const TIME *time);

#ifdef __cplusplus
}
#endif

#endif
