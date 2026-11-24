#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "stm32f103xb.h"
#include "aht20.h"
#include "bmp280.h"
#include "board_i2c.h"
#include "menu.h"
#include "mq5.h"
#include "rtc.h"
#include "threshold_store.h"
#include "uartdma.h"

#include <math.h>
#include <stdio.h>

namespace
{
constexpr TickType_t sample_period = pdMS_TO_TICKS(1000);
constexpr TickType_t display_retry_period = pdMS_TO_TICKS(1000);
constexpr TickType_t button_period = pdMS_TO_TICKS(5);

QueueHandle_t button_queue;
Menu menu;
Sh1106 oled;

void send_line(const char *text, int length, size_t capacity)
{
    if (length > 0 && static_cast<size_t>(length) < capacity)
        dma_uart_writebytes(reinterpret_cast<const uint8_t *>(text), static_cast<uint16_t>(length));
}

template<size_t N>
void send_message(const char (&text)[N])
{
    send_line(text, N - 1U, N);
}

void init_uart()
{
    constexpr uint32_t baud = 115200U;
    const uint32_t prescaler = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    const uint32_t pclk2 = SystemCoreClock >> (prescaler < 4U ? 0U : prescaler - 3U);
    dma_uart(baud);
    USART1->CR1 &= ~USART_CR1_UE;
    USART1->CR2 &= ~(USART_CR2_STOP | USART_CR2_CLKEN);
    USART1->BRR = (pclk2 + baud / 2U) / baud;
    USART1->CR1 |= USART_CR1_UE;
    send_message("STM32: USART1 115200 8N1\r\n");
}

void init_timer2()
{
    const uint32_t prescaler = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    const uint32_t pclk1 = SystemCoreClock >> (prescaler < 4U ? 0U : prescaler - 3U);
    const uint32_t timer_clock = prescaler < 4U ? pclk1 : 2U * pclk1;
    const uint32_t divider = timer_clock / 1000U;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;
    TIM2->CR1 = 0U;
    TIM2->PSC = divider > 65536U ? 65535U : divider - 1U;
    TIM2->ARR = 65535U;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CR1 = TIM_CR1_CEN;
}

int oled_write(uint8_t address, const uint8_t *data, uint16_t size, void *)
{
    return Board_I2CWrite(address, data, size) ? 0 : -1;
}

void delay_ms(uint32_t ms, void *)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

bool init_oled()
{
    return SH1106_Init(&oled, oled_write, nullptr, SH1106_ADDR7, delay_ms) == 0 ||
           SH1106_Init(&oled, oled_write, nullptr, 0x3DU, delay_ms) == 0;
}

int save_threshold(MenuLimit id, int32_t value, void *user)
{
    const Menu *display_menu = static_cast<const Menu *>(user);
    int32_t thresholds[MENU_LIMIT_COUNT];
    for (unsigned i = 0; i < MENU_LIMIT_COUNT; ++i)
        thresholds[i] = Menu_GetThreshold(display_menu, static_cast<MenuLimit>(i));
    thresholds[id] = value;
    return ThresholdStore_Save(thresholds);
}

int write_rtc(const MenuDateTime *value, void *)
{
    // RTC can thu (1 = Chu nhat), menu chi chinh ngay va gio.
    static constexpr uint8_t offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    const unsigned year = value->year - (value->month < 3U ? 1U : 0U);
    const auto weekday = static_cast<uint8_t>(
        (year + year / 4U - year / 100U + year / 400U +
         offsets[value->month - 1U] + value->day) % 7U + 1U);
    const DATE date = {value->day, value->month, value->year, weekday};
    const TIME time = {value->hour, value->minute, value->second};
    return setRTC(&date, &time) ? 0 : -1;
}

void publish_scaled(MenuSensor sensor, float value)
{
    const bool valid = isfinite(value);
    Menu_SetSensor(&menu, sensor, valid ? lroundf(value) : 0, valid);
}

void update_sensors()
{
    char text[96];
    float temperature, humidity;
    readAHT20(&temperature, &humidity);
    publish_scaled(MENU_AHT_TEMP, temperature * 10.0f);
    publish_scaled(MENU_AHT_HUM, humidity * 10.0f);
    int length;
    const AHT20_Error aht_error = aht20_get_last_error();
    if (aht_error == AHT20_ERROR_NONE)
        length = snprintf(text, sizeof(text), "Temp: %.2f C, Humi: %.2f %%\r\n",
                          static_cast<double>(temperature), static_cast<double>(humidity));
    else
        length = snprintf(text, sizeof(text), "AHT20: error=%u\r\n",
                          static_cast<unsigned>(aht_error));
    send_line(text, length, sizeof(text));

    static bool bmp_ready = false;
    if (!bmp_ready)
        bmp_ready = bmp280_init() != 0;
    float bmp_temperature = NAN;
    const float pressure_pa = bmp_ready ? readBmp280Sample(&bmp_temperature) : NAN;
    bmp_ready = isfinite(pressure_pa);
    publish_scaled(MENU_BMP_TEMP, bmp_temperature * 10.0f);
    publish_scaled(MENU_BMP_PRESS, pressure_pa / 10.0f); // Pa -> hPa x10.
    if (bmp_ready)
        length = snprintf(text, sizeof(text), "BMP280: %.2f Pa\r\n", static_cast<double>(pressure_pa));
    else
        length = snprintf(text, sizeof(text), "BMP280: error=%u\r\n",
                          static_cast<unsigned>(bmp280_get_last_error()));
    send_line(text, length, sizeof(text));

    uint16_t gas_adc = 0;
    const bool gas_valid = readMQ5Raw(&gas_adc);
    Menu_SetSensor(&menu, MENU_MQ5_ADC, gas_adc, gas_valid);
    if (gas_valid)
        length = snprintf(text, sizeof(text), "MQ5: %.2f\r\n", gas_adc * (5.5 / 4095.0));
    else {
        length = snprintf(text, sizeof(text), "MQ5: ADC timeout\r\n");
        MQ5_Init();
    }
    send_line(text, length, sizeof(text));

    static bool rtc_ready = false;
    if (!rtc_ready)
        rtc_ready = rtc_init();
    DATE date;
    TIME time;
    if (rtc_ready && readRTC(&date, &time)) {
        const MenuDateTime clock = {date.year, date.month, date.day, time.hour, time.minute, time.second};
        Menu_SetRTC(&menu, &clock);
    } else {
        rtc_ready = false;
        Menu_SetRTC(&menu, nullptr);
    }
}

void button_task(void *)
{
    ButtonManager buttons;
    Button_Init(&buttons, 25U);
    for (;;) {
        const uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        const ButtonEvent event = Button_Poll(&buttons, now_ms);
        if (event != BUTTON_NONE)
            xQueueSend(button_queue, &event, portMAX_DELAY);
        vTaskDelay(button_period);
    }
}

void application_task(void *)
{
    // Mot task so huu I2C1, gom OLED, AHT20, BMP280 va RTC.
    const MenuHooks hooks = {save_threshold, write_rtc, &menu};
    Menu_Init(&menu, &oled, &hooks);
    int32_t stored[MENU_LIMIT_COUNT];
    if (ThresholdStore_Load(stored) == 0) {
        for (unsigned i = 0; i < MENU_LIMIT_COUNT; ++i)
            Menu_SetThreshold(&menu, static_cast<MenuLimit>(i), stored[i]);
    }

    MQ5_Init();
    bool oled_ready = false;
    TickType_t last_sample = xTaskGetTickCount() - sample_period;
    TickType_t last_display_attempt = xTaskGetTickCount() - display_retry_period;
    for (;;) {
        const TickType_t now = xTaskGetTickCount();
        ButtonEvent event;
        while (xQueueReceive(button_queue, &event, 0) == pdPASS)
            Menu_HandleButton(&menu, event);

        if (!oled_ready && now - last_display_attempt >= display_retry_period) {
            last_display_attempt = now;
            oled_ready = Board_I2CInit() == 0 && init_oled();
            menu.dirty = 1;
        }
        if (now - last_sample >= sample_period) {
            last_sample = now;
            update_sensors();
        }
        if (oled_ready && Menu_Render(&menu) < 0) {
            oled_ready = false;
            last_display_attempt = xTaskGetTickCount();
        }
        vTaskDelay(button_period);
    }
}
}

int main(void)
{
    SystemCoreClockUpdate();
    NVIC_SetPriorityGrouping(0U);
    init_uart();
    init_timer2();

    button_queue = xQueueCreate(12, sizeof(ButtonEvent));
    if (button_queue &&
        xTaskCreate(button_task, "Buttons", 192, nullptr, 2, nullptr) == pdPASS &&
        xTaskCreate(application_task, "Application", 768, nullptr, 1, nullptr) == pdPASS) {
        vTaskStartScheduler();
    }

    send_message("Application: FreeRTOS start failed\r\n");
    for (;;) {}
}
