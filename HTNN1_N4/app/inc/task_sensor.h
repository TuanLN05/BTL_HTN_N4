#ifndef TASK_SENSOR_H
#define TASK_SENSOR_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C"
#endif

/**
 * @brief Tao TaskSensor de doc cam bien va gui mau vao queue.
 * @retval pdPASS Tao task thanh cong.
 * @retval errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY Khong du bo nho de tao task.
 * @pre IPC da duoc khoi tao.
 */
BaseType_t task_sensor_create(void);

#endif
