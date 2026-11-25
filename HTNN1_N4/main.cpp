#include "FreeRTOS.h"
#include "task.h"

#include "app.h"
#include "app_log.h"
#include "board.h"
#include "task_alarm.h"
#include "task_sensor.h"
#include "task_ui.h"

/**
 * @brief Khoi tao board va IPC, tao task theo cau hinh build va chay FreeRTOS.
 * @note Khi scheduler khong chay, ham o lai trong vong lap cuoi.
 */
int main(void)
{
    Board_Init();
    if (App_Init()) {
#if defined(APP_MOCK_SENSOR)
        const bool tasks_created = task_sensor_create() == pdPASS;
#elif defined(APP_MOCK_ALARM)
        const bool tasks_created = task_alarm_create() == pdPASS;
#elif defined(APP_MOCK_UI)
        const bool tasks_created = task_ui_create() == pdPASS;
#else
        const bool tasks_created = task_sensor_create() == pdPASS &&
                                   task_alarm_create() == pdPASS &&
                                   task_ui_create() == pdPASS;
#endif
        if (tasks_created)
            vTaskStartScheduler();
    }
    while(1){}
}
