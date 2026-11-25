#include "board.h"
#include "board_pins.h"
#include "hal_gpio.h"
#include "hal_uart.h"

namespace
{
const uint8_t button_pins[] = {
    BOARD_BUTTON_UP_PIN, BOARD_BUTTON_DOWN_PIN, BOARD_BUTTON_SELECT_PIN
};

/**
 * @brief Khoi tao USART1 115200 8N1 theo clock APB2 thuc te va gui log khoi dong.
 */
void init_uart()
{
    constexpr uint32_t baud = BOARD_UART_BAUD;
    const uint32_t prescaler = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    const uint32_t pclk2 = SystemCoreClock >> (prescaler < 4U ? 0U : prescaler - 3U);
    dma_uart(baud);
    USART1->CR1 &= ~USART_CR1_UE;
    USART1->CR2 &= ~(USART_CR2_STOP | USART_CR2_CLKEN);
    USART1->BRR = (pclk2 + baud / 2U) / baud;
    USART1->CR1 |= USART_CR1_UE;
    const char message[] = "STM32: USART1 115200 8N1\r\n";
    dma_uart_writebytes(reinterpret_cast<const uint8_t *>(message), sizeof(message) - 1U);
}

/**
 * @brief Cau hinh TIM2 dem theo buoc 1 ms cho timeout I2C1.
 */
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
}

void Board_Init(void)
{
    SystemCoreClockUpdate();
    NVIC_SetPriorityGrouping(0U);
    (void)gpio_Config(BOARD_ALARM_LED_PORT, BOARD_ALARM_LED_PIN, GPIO_OUTPUT_LOW);
    init_uart();
    init_timer2();
}

void Board_SetAlarmLed(bool on)
{
    if (on)
        gpio_Set(BOARD_ALARM_LED_PORT, BOARD_ALARM_LED_PIN);
    else
        gpio_Reset(BOARD_ALARM_LED_PORT, BOARD_ALARM_LED_PIN);
}

void Board_ButtonsInit(void)
{
    for (uint8_t pin : button_pins)
        (void)gpio_Config(BOARD_BUTTON_PORT, pin, GPIO_INPUT_PULLUP);
}

uint8_t Board_ButtonRead(uint8_t index)
{
    if (index >= sizeof(button_pins) / sizeof(button_pins[0]))
        return 0;
    return gpio_Read(BOARD_BUTTON_PORT, button_pins[index]) == 0;
}
