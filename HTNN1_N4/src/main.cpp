#include "stm32f103xb.h"
#include "board_i2c.h"
#include "menu.h"
#include "threshold_store.h"
#include <stdbool.h>
#include <stddef.h>

#define OLED_I2C_TIMEOUT_MS 25U
#define OLED_RETRY_MS 1000U
#define I2C_ERRORS (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR | I2C_SR1_TIMEOUT)

typedef enum {
    DEMO_NORMAL,
    DEMO_ALARM,
    DEMO_NO_DATA
} DemoSample;

typedef struct {
    Menu menu;
    MenuDateTime clock;
    uint32_t clock_tick_ms;
    DemoSample sample;
} Demo;

static volatile uint32_t milliseconds;
static Demo demo;
static Sh1106 oled;
static ButtonManager buttons;

extern "C" void SysTick_Handler(void)
{
    ++milliseconds;
}

static uint32_t start_timebase(void)
{
    SystemCoreClockUpdate();
    (void)SysTick_Config(SystemCoreClock / 1000U);

    const uint32_t prescaler = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    const uint32_t pclk1 = SystemCoreClock >> (prescaler < 4U ? 0U : prescaler - 3U);
    const uint32_t tim2_clock = prescaler < 4U ? pclk1 : 2U * pclk1;
    const uint32_t divider = tim2_clock / 1000U;

    // Board_I2CInit dung TIM2 de tao khoang cho khi phuc hoi bus.
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;
    TIM2->CR1 = 0U;
    TIM2->PSC = divider > 65536U ? 65535U : divider - 1U;
    TIM2->ARR = 65535U;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CR1 = TIM_CR1_CEN;
    return pclk1;
}

static void delay_ms(uint32_t ms, void *user)
{
    (void)user;
    const uint32_t start = milliseconds;
    while (milliseconds - start < ms) {}
}

static bool wait_i2c(volatile uint32_t *reg, uint32_t mask, bool set)
{
    const uint32_t start = milliseconds;
    do {
        if (I2C1->SR1 & I2C_ERRORS)
            return false;
        if (((*reg & mask) != 0U) == set)
            return true;
    } while (milliseconds - start < OLED_I2C_TIMEOUT_MS);
    return false;
}

// SH1106 dung callback ghi co timeout de OLED thieu ACK khong lam dung vong lap.
static int oled_write(uint8_t address, const uint8_t *data, uint16_t length, void *user)
{
    (void)user;
    const uint32_t pending = I2C_CR1_START | I2C_CR1_STOP;
    if (!wait_i2c(&I2C1->CR1, pending, false) ||
        !wait_i2c(&I2C1->SR2, I2C_SR2_BUSY, false))
        goto failed;

    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_i2c(&I2C1->SR1, I2C_SR1_SB, true))
        goto failed;
    I2C1->DR = (uint32_t)address << 1U;
    if (!wait_i2c(&I2C1->SR1, I2C_SR1_ADDR, true))
        goto failed;
    (void)I2C1->SR1;
    (void)I2C1->SR2;

    for (uint16_t i = 0; i < length; ++i) {
        if (!wait_i2c(&I2C1->SR1, I2C_SR1_TXE, true))
            goto failed;
        I2C1->DR = data[i];
    }
    if (!wait_i2c(&I2C1->SR1, I2C_SR1_BTF, true))
        goto failed;
    I2C1->CR1 |= I2C_CR1_STOP;
    if (wait_i2c(&I2C1->CR1, I2C_CR1_STOP, false))
        return 0;

failed:
    if ((I2C1->SR2 & I2C_SR2_MSL) && !(I2C1->CR1 & pending))
        I2C1->CR1 |= I2C_CR1_STOP;
    I2C1->SR1 &= ~I2C_ERRORS;
    (void)wait_i2c(&I2C1->CR1, pending, false);
    return -1;
}

static bool init_oled(uint32_t pclk1)
{
    if (pclk1 < 2000000U || pclk1 > 36000000U || Board_I2CInit() != 0)
        return false;

    // Giu PB6/PB7 cua board va tinh timing I2C theo clock APB1.
    I2C1->CR1 &= ~I2C_CR1_PE;
    I2C1->CR2 = pclk1 / 1000000U;
    I2C1->CCR = (pclk1 + 159999U) / 160000U; // SCL 80 kHz.
    I2C1->TRISE = pclk1 / 1000000U + 1U;
    I2C1->OAR1 = 1U << 14U;
    I2C1->CR1 = I2C_CR1_PE;
    return SH1106_Init(&oled, oled_write, NULL, SH1106_ADDR7, delay_ms) == 0 ||
           SH1106_Init(&oled, oled_write, NULL, 0x3DU, delay_ms) == 0;
}

static int save_threshold(MenuLimit id, int32_t value, void *user)
{
    const Demo *app = static_cast<const Demo *>(user);
    int32_t values[MENU_LIMIT_COUNT];
    for (unsigned i = 0; i < MENU_LIMIT_COUNT; ++i)
        values[i] = Menu_GetThreshold(&app->menu, (MenuLimit)i);
    values[id] = value;
    return ThresholdStore_Save(values);
}

static int set_demo_clock(const MenuDateTime *time, void *user)
{
    Demo *app = static_cast<Demo *>(user);
    app->clock = *time;
    app->clock_tick_ms = milliseconds;
    return 0;
}

static void increment_second(MenuDateTime *time)
{
    static const uint8_t month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (++time->second < 60U) return;
    time->second = 0U;
    if (++time->minute < 60U) return;
    time->minute = 0U;
    if (++time->hour < 24U) return;
    time->hour = 0U;
    uint8_t days = month_days[time->month - 1U];
    if (time->month == 2U && time->year % 4U == 0U) ++days;
    if (++time->day <= days) return;
    time->day = 1U;
    if (++time->month <= 12U) return;
    time->month = 1U;
    time->year = time->year == 2099U ? 2000U : (uint16_t)(time->year + 1U);
}

static void publish_sample(Demo *app)
{
    static const int32_t normal[MENU_SENSOR_COUNT] = {260, 710, 253, 10080, 950};
    static const MenuSensor limit_sensors[MENU_LIMIT_COUNT] =
        {MENU_AHT_TEMP, MENU_AHT_HUM, MENU_BMP_PRESS, MENU_MQ5_ADC};

    int32_t values[MENU_SENSOR_COUNT];
    for (unsigned i = 0; i < MENU_SENSOR_COUNT; ++i)
        values[i] = normal[i];
    if (app->sample == DEMO_ALARM) {
        for (unsigned i = 0; i < MENU_LIMIT_COUNT; ++i)
            values[limit_sensors[i]] = Menu_GetThreshold(&app->menu, (MenuLimit)i);
    }
    for (unsigned i = 0; i < MENU_SENSOR_COUNT; ++i)
        Menu_SetSensor(&app->menu, (MenuSensor)i, values[i], app->sample != DEMO_NO_DATA);
}

static void handle_button(Demo *app, ButtonEvent event)
{
    // Hai phim chua dung o HOME duoc dung de doi du lieu test.
    if (app->menu.page == MENU_PAGE_HOME && !app->menu.popup_visible &&
        (event == BUTTON_UP || event == BUTTON_DOWN)) {
        const DemoSample target = event == BUTTON_UP ? DEMO_ALARM : DEMO_NO_DATA;
        app->sample = app->sample == target ? DEMO_NORMAL : target;
        publish_sample(app);
    } else {
        Menu_HandleButton(&app->menu, event);
    }
}

int main(void)
{
    const uint32_t pclk1 = start_timebase();
    Button_Init(&buttons, 25U);

    const MenuHooks hooks = {save_threshold, set_demo_clock, &demo};
    Menu_Init(&demo.menu, &oled, &hooks);

    int32_t stored[MENU_LIMIT_COUNT];
    if (ThresholdStore_Load(stored) == 0) {
        for (unsigned i = 0; i < MENU_LIMIT_COUNT; ++i)
            Menu_SetThreshold(&demo.menu, (MenuLimit)i, stored[i]);
    }

    const MenuDateTime initial_clock = {2028U, 2U, 28U, 23U, 59U, 50U};
    set_demo_clock(&initial_clock, &demo);
    Menu_SetRTC(&demo.menu, &demo.clock);
    publish_sample(&demo);

    bool oled_ready = false;
    uint32_t last_attempt = milliseconds - OLED_RETRY_MS;
    for (;;) {
        const uint32_t now = milliseconds;
        while (now - demo.clock_tick_ms >= 1000U) {
            demo.clock_tick_ms += 1000U;
            increment_second(&demo.clock);
        }
        Menu_SetRTC(&demo.menu, &demo.clock);
        handle_button(&demo, Button_Poll(&buttons, now));

        if (!oled_ready && now - last_attempt >= OLED_RETRY_MS) {
            last_attempt = now;
            oled_ready = init_oled(pclk1);
            demo.menu.dirty = 1U;
        }
        if (oled_ready && Menu_Render(&demo.menu) < 0) {
            oled_ready = false;
            last_attempt = milliseconds;
        }
        delay_ms(5U, NULL);
    }
}
