#ifndef TASK_ALARM_H
#define TASK_ALARM_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C"
#endif

/**
 * @brief Tao TaskAlarm de so sanh nguong, dieu khien PA1 va gui ket qua cho UI.
 * @retval pdPASS Tao task thanh cong.
 * @retval errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY Khong du bo nho de tao task.
 * @pre IPC da duoc khoi tao.
 */
BaseType_t task_alarm_create(void);

#endif
