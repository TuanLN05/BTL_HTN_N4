#include "task_alarm.h"
#include "task.h"
#include "app_ipc.h"
#include "board.h"

static const MenuSensor limit_source[MENU_LIMIT_COUNT] = {
    MENU_AHT_TEMP, MENU_AHT_HUM, MENU_BMP_PRESS, MENU_MQ5_ADC
};

/**
 * @brief So sanh tung so do hop le voi nguong tuong ung.
 * @param[in] data Mau cam bien va mat na hop le.
 * @param[in] thresholds Mang nguong theo MenuLimit.
 * @return Mat na cac gia tri hop le lon hon hoac bang nguong.
 */
static uint8_t evaluate_alarms(const SensorData_t *data, const int32_t thresholds[MENU_LIMIT_COUNT])
{
    uint8_t alarms = 0;
    for (unsigned i = 0; i < MENU_LIMIT_COUNT; ++i) {
        const MenuSensor sensor = limit_source[i];
        if ((data->valid_mask & (1U << sensor)) && data->sensor[sensor] >= thresholds[i])
            alarms |= (uint8_t)(1U << i);
    }
    return alarms;
}

/**
 * @brief Cho mau cam bien, tinh canh bao, dieu khien LED va gui ket qua cho UI.
 * @param[in] parameters Tham so FreeRTOS, khong su dung.
 */
static void prvAlarmTask(void *parameters)
{
    (void)parameters;
    AlarmData_t result = {0};
    int32_t thresholds[MENU_LIMIT_COUNT];
#ifdef APP_MOCK_ALARM
    unsigned phase = 0;
#endif

    for (;;) {
#ifdef APP_MOCK_ALARM
        App_GetThresholds(thresholds);
        // Chu ky 3 mau: duoi nguong, nhiet do va gas vuot nguong, mau loi.
        SensorData_t mock = {0};
        mock.sensor[MENU_AHT_TEMP] = thresholds[MENU_LIMIT_TEMP] + (phase == 0 ? -1 : 1);
        mock.sensor[MENU_MQ5_ADC] = thresholds[MENU_LIMIT_GAS] + (phase == 0 ? -1 : 1);
        if (phase != 2)
            mock.valid_mask = (1U << MENU_AHT_TEMP) | (1U << MENU_MQ5_ADC);
        App_IPCSendSensor(&mock);
#endif
        App_IPCReceiveSensor(&result.data);
        App_GetThresholds(thresholds);
        result.alarm_mask = evaluate_alarms(&result.data, thresholds);
        Board_SetAlarmLed(result.alarm_mask != 0);
        App_IPCPublishAlarm(&result);
#ifdef APP_MOCK_ALARM
        phase = (phase + 1) % 3;
        vTaskDelay(pdMS_TO_TICKS(2000));
#endif
    }
}

BaseType_t task_alarm_create(void)
{
    return xTaskCreate(prvAlarmTask, "ALARM_TASK", 192, NULL, tskIDLE_PRIORITY + 3, NULL);
}
