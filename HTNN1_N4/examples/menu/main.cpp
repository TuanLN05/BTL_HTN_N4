#include "stm32f1xx.h"
#include "i2c.h"
#include "button.h"
#include "menu.h"
#include "threshold_store.h"

static Sh1106 oled;
static ButtonManager buttons;
static Menu menu;
volatile uint32_t g_oled_stage;
volatile int g_oled_result;

// TIM2 dem 1 ms; khong phu thuoc ngat.
static uint32_t NowMs(void)
{
    static uint16_t previous;
    static uint32_t elapsed;
    uint16_t tick = (uint16_t)TIM2->CNT;
    elapsed += (uint16_t)(tick - previous);
    previous = tick;
    return elapsed;
}

static void DelayMs(uint32_t ms, void *user)
{
    uint32_t start = NowMs();
    (void)user;
    while ((uint32_t)(NowMs() - start) < ms) {}
}

// HSI / 2 * 4 = 16 MHz; AHB, APB1, APB2 khong chia.
static void Clock_Init(void)
{
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {}
    RCC->CFGR &= ~RCC_CFGR_SW;
    while (RCC->CFGR & RCC_CFGR_SWS) {}
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) {}
    RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE |
                   RCC_CFGR_PLLMULL | RCC_CFGR_HPRE |
                   RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    RCC->CFGR |= RCC_CFGR_PLLMULL4;
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}
    SystemCoreClockUpdate();
}

static int OLED_Write(uint8_t addr, const uint8_t *data,
                      uint16_t length, void *user)
{
    uint8_t tx[33];
    (void)user;
    if (addr > 0x7F || !data || !length || length > sizeof(tx)) return -1;
    for (uint16_t i = 0; i < length; ++i) tx[i] = data[i];
    g_oled_stage = 5;
    i2c1_writeBytes(addr, tx, (uint8_t)length);
    g_oled_stage = 6;
    return 0;
}

static int SaveThreshold(MenuLimit id, int32_t value, void *user)
{
    int32_t values[MENU_LIMIT_COUNT];
    (void)user;
    if ((unsigned)id >= MENU_LIMIT_COUNT) return -1;
    for (unsigned i = 0; i < MENU_LIMIT_COUNT; ++i)
        values[i] = Menu_GetThreshold(&menu, (MenuLimit)i);
    values[id] = value;
    return ThresholdStore_Save(values);
}

// Khoi tao mot lan truoc vong lap hien thi menu.
static int App_Init(void)
{
    int32_t saved[MENU_LIMIT_COUNT];
    const MenuHooks hooks = {SaveThreshold, 0, 0};
    g_oled_stage = 1;
    Clock_Init();
    g_oled_stage = 2;
    SysTick->CTRL = 0;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    TIM2->CR1 = 0;
    TIM2->PSC = SystemCoreClock / 1000U - 1U;
    TIM2->ARR = 0xFFFFU;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    TIM2->CNT = 0;
    TIM2->CR1 = TIM_CR1_CEN;
    i2c1_init();
    Button_Init(&buttons, 25);
    g_oled_stage = 3;
    DelayMs(100, 0);
    g_oled_stage = 4;
    g_oled_result = SH1106_Init(&oled, OLED_Write, 0, SH1106_ADDR7, DelayMs);
    if (g_oled_result != 0)
        return -1;
    g_oled_stage = 7;
    Menu_Init(&menu, &oled, &hooks);
    if (ThresholdStore_Load(saved) == 0) {
        for (unsigned i = 0; i < MENU_LIMIT_COUNT; ++i)
            Menu_SetThreshold(&menu, (MenuLimit)i, saved[i]);
    }
    g_oled_result = Menu_Render(&menu);
    if (g_oled_result < 0) return -1;
    g_oled_stage = 8;
    return 0;
}

int main(void)
{
    if (App_Init() != 0) {
        while (1) {}
    }
    while (1) {
        ButtonEvent event = Button_Poll(&buttons, NowMs());
        if (event != BUTTON_NONE) Menu_HandleButton(&menu, event);
        (void)Menu_Render(&menu);
        DelayMs(5, 0);
    }
}
