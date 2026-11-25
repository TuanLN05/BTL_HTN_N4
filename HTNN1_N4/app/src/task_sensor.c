#include "task_sensor.h"
#include "task.h"
#include "app_ipc.h"
#include "app_log.h"
#include "aht20.h"
#include "bmp280.h"
#include "board_i2c.h"
#include "mq5.h"
#include "rtc.h"

#include <math.h>
#include <stdio.h>

/**
 * @brief Ghi mot gia tri va cap nhat bit hop le tuong ung.
 * @param[in,out] data Mau tong hop can cap nhat.
 * @param[in] sensor Chi so so do.
 * @param[in] value Gia tri cua so do.
 * @param[in] valid true neu mau hop le.
 */
static void set_sample(SensorData_t *data, MenuSensor sensor, int32_t value, bool valid)
{
    const uint8_t bit = (uint8_t)(1U << sensor);
    data->sensor[sensor] = value;
    if (valid)
        data->valid_mask |= bit;
    else
        data->valid_mask &= (uint8_t)~bit;
}

/**
 * @brief Lam tron mot gia tri da doi don vi va danh dau NAN hoac vo cuc la mau loi.
 * @param[in,out] data Mau tong hop can cap nhat.
 * @param[in] sensor Chi so so do.
 * @param[in] value Gia tri thuc da quy doi.
 */
static void set_scaled_sample(SensorData_t *data, MenuSensor sensor, float value)
{
    const bool valid = isfinite(value);
    set_sample(data, sensor, valid ? lroundf(value) : 0, valid);
}

/**
 * @brief Nha mutex I2C1 trong luc cho cam bien va lay lai truoc khi tiep tuc.
 * @param[in] ms Thoi gian cho toi thieu tinh bang ms.
 * @pre TaskSensor dang giu mutex I2C1.
 */
static void wait_for_conversion(uint32_t ms)
{
    // Cac driver chi goi callback sau STOP, khi cam bien dang tu chuyen doi.
    App_I2CGive();
    vTaskDelay(pdMS_TO_TICKS(ms) + 1U);
    App_I2CTake(portMAX_DELAY);
}

/**
 * @brief Doc DS1307 va danh dau loi de khoi tao lai o lan doc sau.
 * @param[in,out] data Mau tong hop chua ngay gio va co hop le.
 * @pre TaskSensor dang giu mutex I2C1.
 */
static void read_rtc(SensorData_t *data)
{
    static bool rtc_ready;
    DATE date;
    TIME time;
    if (!rtc_ready)
        rtc_ready = rtc_init();
    data->rtc_valid = rtc_ready && readRTC(&date, &time);
    if (data->rtc_valid)
        data->rtc = (MenuDateTime){date.year, date.month, date.day, time.hour, time.minute, time.second};
    else
        rtc_ready = false;
}

typedef struct {
    float temperature, humidity, pressure_pa;
} SlowReadings;

/**
 * @brief Doc RTC, AHT20 va BMP280, sau do cap nhat mau tong hop.
 * @param[in,out] data Mau tong hop nhan cac so do va co hop le.
 * @return Nhiet do, do am va ap suat truoc khi lam tron de ghi log.
 */
static SlowReadings read_slow_sensors(SensorData_t *data)
{
    static bool bus_ready;
    static bool bmp_ready;
    SlowReadings sample = {NAN, NAN, NAN};
    float bmp_temperature = NAN;

    App_I2CTake(portMAX_DELAY);
    if (!bus_ready)
        bus_ready = Board_I2CInit() == 0;
    if (bus_ready) {
        read_rtc(data);
        readAHT20(&sample.temperature, &sample.humidity);
        if (!bmp_ready)
            bmp_ready = bmp280_init() != 0;
        if (bmp_ready)
            sample.pressure_pa = readBmp280Sample(&bmp_temperature);
        bmp_ready = isfinite(sample.pressure_pa);
    }
    App_I2CGive();

    set_scaled_sample(data, MENU_AHT_TEMP, sample.temperature * 10.0f);
    set_scaled_sample(data, MENU_AHT_HUM, sample.humidity * 10.0f);
    set_scaled_sample(data, MENU_BMP_TEMP, bmp_temperature * 10.0f);
    set_scaled_sample(data, MENU_BMP_PRESS, sample.pressure_pa / 10.0f);
    return sample;
}

/**
 * @brief Gui mau qua IPC va doc lai de ghi log trong ban mock_sensor.
 * @param[in] data Mau tong hop can gui.
 */
static void publish_sample(const SensorData_t *data)
{
    App_IPCSendSensor(data);
#ifdef APP_MOCK_SENSOR
    // Doc lai queue ngay trong task khi chay doc lap.
    SensorData_t received;
    App_IPCReceiveSensor(&received);
    char text[64];
    const int length = snprintf(text, sizeof(text), "SENSOR QUEUE: mask=0x%02X MQ5=%ld ADC\r\n",
                                (unsigned)received.valid_mask, (long)received.sensor[MENU_MQ5_ADC]);
    App_LogLine(text, length, sizeof(text));
#endif
}

/**
 * @brief Ghi so do va trang thai loi len UART.
 * @param[in] data Mau tong hop chua ADC va cac co hop le.
 * @param[in] sample Cac so do thuc truoc khi lam tron.
 */
static void log_slow_sample(const SensorData_t *data, const SlowReadings *sample)
{
    char text[96];
    int length;
    if (data->valid_mask & (1U << MENU_AHT_TEMP))
        length = snprintf(text, sizeof(text), "Temp: %.2f C, Humi: %.2f %%\r\n",
                          (double)sample->temperature, (double)sample->humidity);
    else
        length = snprintf(text, sizeof(text), "AHT20: no valid reading (error=%u)\r\n",
                          (unsigned)aht20_get_last_error());
    App_LogLine(text, length, sizeof(text));

    if (data->valid_mask & (1U << MENU_BMP_PRESS))
        length = snprintf(text, sizeof(text), "BMP280: %.2f Pa\r\n",
                          (double)sample->pressure_pa);
    else
        length = snprintf(text, sizeof(text), "BMP280: no valid reading (error=%u)\r\n",
                          (unsigned)bmp280_get_last_error());
    App_LogLine(text, length, sizeof(text));
#ifdef APP_MOCK_SENSOR
    if (data->rtc_valid) {
        const MenuDateTime rtc = data->rtc;
        length = snprintf(text, sizeof(text), "RTC: %04u-%02u-%02u %02u:%02u:%02u\r\n",
                          (unsigned)rtc.year, (unsigned)rtc.month, (unsigned)rtc.day,
                          (unsigned)rtc.hour, (unsigned)rtc.minute, (unsigned)rtc.second);
        App_LogLine(text, length, sizeof(text));
    } else {
        App_LogMessage("RTC: no valid reading\r\n");
    }
#endif

    if (data->valid_mask & (1U << MENU_MQ5_ADC))
        length = snprintf(text, sizeof(text), "MQ5: %.2f\r\n",
                          data->sensor[MENU_MQ5_ADC] * (5.5 / 4095.0));
    else
        length = snprintf(text, sizeof(text), "MQ5: ADC timeout\r\n");
    App_LogLine(text, length, sizeof(text));
}

/**
 * @brief Doc MQ5 theo chu ky 125 ms va doc AHT20, BMP280, RTC theo chu ky 1 giay.
 * @param[in] parameters Tham so FreeRTOS, khong su dung.
 */
static void prvSensorTask(void *parameters)
{
    (void)parameters;
    const TickType_t gas_period = pdMS_TO_TICKS(125);
    const TickType_t slow_period = pdMS_TO_TICKS(1000);
    SensorData_t data = {0};

    MQ5_Init();
    aht20_set_delay(wait_for_conversion);
    bmp280_set_delay(wait_for_conversion);
    TickType_t last_wake = xTaskGetTickCount();
    TickType_t last_slow = last_wake - slow_period;

    for (;;) {
        const TickType_t now = xTaskGetTickCount();
        uint16_t gas_adc = 0;
        const bool valid = readMQ5Raw(&gas_adc);
        set_sample(&data, MENU_MQ5_ADC, gas_adc, valid);
        publish_sample(&data);
        if (!valid)
            MQ5_Init();

        if (now - last_slow >= slow_period) {
            last_slow = now;
            const SlowReadings slow = read_slow_sensors(&data);
            publish_sample(&data);
            log_slow_sample(&data, &slow);
        }

        // Bo qua cac chu ky da lo khi khoi tao lai hoac bus bi loi.
        const TickType_t elapsed = xTaskGetTickCount() - last_wake;
        last_wake += (elapsed / gas_period) * gas_period;
        vTaskDelayUntil(&last_wake, gas_period);
    }
}

BaseType_t task_sensor_create(void)
{
    return xTaskCreate(prvSensorTask, "SENSOR_TASK", 640, NULL, tskIDLE_PRIORITY + 2, NULL);
}
