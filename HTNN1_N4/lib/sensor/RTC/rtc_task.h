#ifndef RTC_TASK_H
#define RTC_TASK_H

#include "FreeRTOS.h"
#include "rtc.h"

#ifdef __cplusplus
extern "C" {
#endif

// Luu mau thoi gian doc thanh cong gan nhat
// Task khac sao chep thoi gian va trang thai trong vung bao ve ngan
extern volatile RTC_Time rtc_time;
extern volatile uint8_t rtc_read_ok;
extern volatile uint8_t rtc_error;
extern volatile uint32_t rtc_read_count;

// Chi gui yeu cau moi khi rtc_set_request = 0
// Dien du rtc_set_time roi dat rtc_set_request = 1 trong vung bao ve
extern volatile RTC_Time rtc_set_time;
extern volatile uint8_t rtc_set_request;
extern volatile uint8_t rtc_set_ok;
extern volatile uint8_t rtc_set_error;

// Tao task RTC
BaseType_t RTC_TaskStart(void);

#ifdef __cplusplus
}
#endif

#endif
