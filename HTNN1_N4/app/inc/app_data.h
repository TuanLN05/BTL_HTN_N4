#ifndef APP_DATA_H
#define APP_DATA_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MENU_AHT_TEMP, MENU_AHT_HUM, MENU_BMP_TEMP, MENU_BMP_PRESS, MENU_MQ5_ADC,
    MENU_SENSOR_COUNT
} MenuSensor;

typedef enum {
    MENU_LIMIT_TEMP, MENU_LIMIT_HUM, MENU_LIMIT_PRESS, MENU_LIMIT_GAS,
    MENU_LIMIT_COUNT
} MenuLimit;

typedef struct {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
} MenuDateTime;

// Temperature, humidity and pressure are scaled by 10. MQ5 is raw ADC.
typedef struct {
    int32_t sensor[MENU_SENSOR_COUNT];
    uint8_t valid_mask;
    MenuDateTime rtc;
    bool rtc_valid;
} SensorData_t;

typedef struct {
    SensorData_t data;
    uint8_t alarm_mask; // Bit index is MenuLimit.
} AlarmData_t;

#endif
