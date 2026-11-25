#ifndef TASK_UI_H
#define TASK_UI_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C"
#endif

/**
 * @brief Tao TaskUI de doc nut nhan, dieu huong menu va cap nhat OLED.
 * @retval pdPASS Tao task thanh cong.
 * @retval errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY Khong du bo nho de tao task.
 * @pre IPC da duoc khoi tao.
 */
BaseType_t task_ui_create(void);

#endif
