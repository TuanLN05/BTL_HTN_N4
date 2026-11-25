#ifndef APP_IPC_H
#define APP_IPC_H

#include "FreeRTOS.h"
#include "app_data.h"

#ifdef __cplusplus
extern "C" {
#endif

// Tao hai queue du lieu va mutex I2C truoc khi tao task.
bool App_IPCInit(void);
void App_IPCSendSensor(const SensorData_t *data);
void App_IPCReceiveSensor(SensorData_t *data);
void App_IPCPublishAlarm(const AlarmData_t *data);
bool App_IPCReceiveAlarm(AlarmData_t *data);

void App_GetThresholds(int32_t values[MENU_LIMIT_COUNT]);
void App_SetThresholds(const int32_t values[MENU_LIMIT_COUNT]);
bool App_I2CTake(TickType_t timeout);
void App_I2CGive(void);

#ifdef __cplusplus
}
#endif

#endif
