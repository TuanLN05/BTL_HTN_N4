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

/**
 * @brief Kiem tra DS1307 va bat dao dong neu dang dung, giu nguyen ngay gio.
 * @retval true RTC san sang.
 * @retval false Loi giao tiep I2C.
 * @pre I2C1 da duoc khoi tao.
 */
bool rtc_init(void);

/**
 * @brief Doc va kiem tra ngay gio tren DS1307.
 * @param[out] date Ngay, thang, nam va thu.
 * @param[out] time Gio theo he 24 gio, phut va giay.
 * @retval true Da cap nhat ca hai dau ra.
 * @retval false Dau vao rong, du lieu sai hoac loi I2C. Giu nguyen dau ra.
 */
bool readRTC(DATE *date, TIME *time);

/**
 * @brief Ghi ngay gio theo he 24 gio vao DS1307.
 * @param[in] date Ngay, thang, nam 2000-2099 va thu.
 * @param[in] time Gio, phut, giay can ghi.
 * @retval true Da ghi thanh cong.
 * @retval false Ngay gio khong hop le hoac loi I2C.
 */
bool setRTC(const DATE *date, const TIME *time);

#ifdef __cplusplus
}
#endif

#endif
