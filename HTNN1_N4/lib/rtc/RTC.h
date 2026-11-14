#ifndef RTC_H
#define RTC_H

#include <stdint.h>

// Dia chi I2C 7 bit cua DS1307
#define RTC_I2C_ADDRESS 0x68U

// Du lieu thoi gian sau khi giai ma
typedef struct
{
    uint8_t second;
    uint8_t minute;
    // Gio 24h: 0-23
    uint8_t hour;
    // 1 = Chu nhat, ..., 7 = Thu bay
    uint8_t weekday;
    uint8_t day;
    uint8_t month;
    // Nam 2000-2099
    uint16_t year;
} RTC_Time;

// Ngay thang nam dung cho phan hien thi
typedef struct
{
    uint8_t day;
    uint8_t month;
    uint16_t year;
    // 1 = Chu nhat, ..., 7 = Thu bay
    uint8_t weekday;
} DATE;

// Thoi gian theo che do 24 gio
typedef struct
{
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} TIME;

// Ma 0-6 dung chung voi I2C, ma 7-8 rieng cua RTC
enum
{
    RTC_ERROR_NONE = 0,
    RTC_ERROR_NACK = 1,
    RTC_ERROR_BUS = 2,
    RTC_ERROR_TIMEOUT = 3,
    RTC_ERROR_ARGUMENT = 4,
    RTC_ERROR_MUTEX_TIMEOUT = 5,
    RTC_ERROR_MUTEX_NOT_READY = 6,
    RTC_ERROR_INVALID_TIME = 7,
    RTC_ERROR_CLOCK_HALTED = 8
};

// Doc thoi gian, giu nguyen du lieu neu gap loi
// Chi goi tu task RTC sau khi bo lap lich chay, khong goi tu ngat
// Tra ve 1 khi thanh cong, 0 khi loi
uint8_t RTC_Read(RTC_Time *time);

// Dat gio 24h va bat dao dong
// Tra ve 1 khi thanh cong, 0 khi loi
uint8_t RTC_Set(const RTC_Time *time);

// Doc ma loi RTC gan nhat
uint8_t RTC_GetLastError(void);

// Lay mau moi nhat do task RTC doc, khong giao tiep I2C
// Giu nguyen dau ra neu chua co mau, doc loi hoac con tro NULL
// Goi tu task sau khi bo lap lich chay, khong goi tu ngat
void readRTC(DATE *date, TIME *time);

// Giong readRTC, tra ve 1 khi lay duoc mau va 0 khi chua san sang hoac loi
uint8_t RTC_GetSnapshot(DATE *date, TIME *time);

#endif
