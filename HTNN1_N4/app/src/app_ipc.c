#include "app_ipc.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "threshold_store.h"

#include <string.h>

static QueueHandle_t sensor_queue;
static QueueHandle_t alarm_queue;
static SemaphoreHandle_t i2c_mutex;
static int32_t thresholds[MENU_LIMIT_COUNT] = {350, 800, 10500, 2000};

bool App_IPCInit(void)
{
    (void)ThresholdStore_Load(thresholds);
    sensor_queue = xQueueCreate(4, sizeof(SensorData_t));
    alarm_queue = xQueueCreate(1, sizeof(AlarmData_t));
    i2c_mutex = xSemaphoreCreateMutex();
    return sensor_queue && alarm_queue && i2c_mutex;
}

void App_IPCSendSensor(const SensorData_t *data)
{
    xQueueSend(sensor_queue, data, portMAX_DELAY);
}

void App_IPCReceiveSensor(SensorData_t *data)
{
    xQueueReceive(sensor_queue, data, portMAX_DELAY);
}

void App_IPCPublishAlarm(const AlarmData_t *data)
{
    xQueueOverwrite(alarm_queue, data);
}

bool App_IPCReceiveAlarm(AlarmData_t *data)
{
    return xQueueReceive(alarm_queue, data, 0) == pdPASS;
}

void App_GetThresholds(int32_t values[MENU_LIMIT_COUNT])
{
    taskENTER_CRITICAL();
    memcpy(values, thresholds, sizeof(thresholds));
    taskEXIT_CRITICAL();
}

void App_SetThresholds(const int32_t values[MENU_LIMIT_COUNT])
{
    taskENTER_CRITICAL();
    memcpy(thresholds, values, sizeof(thresholds));
    taskEXIT_CRITICAL();
}

bool App_I2CTake(TickType_t timeout)
{
    return xSemaphoreTake(i2c_mutex, timeout) == pdTRUE;
}

void App_I2CGive(void)
{
    xSemaphoreGive(i2c_mutex);
}
