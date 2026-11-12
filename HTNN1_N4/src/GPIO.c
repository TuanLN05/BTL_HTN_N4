#include "stm32f1xx.h"
#include "GPIO.h"

#define FAN_PIN    1U
#define BUZZER_PIN 2U
#define BUTTON_COUNT 3U

typedef struct
{
    uint32_t changed_at;
    uint8_t last_raw;
    uint8_t stable;
    uint8_t initialized;
} ButtonState;

static ButtonState button_states[BUTTON_COUNT];

static void GPIO_Write(uint32_t pin, uint8_t level)
{
    GPIOA->BSRR = level ? (1UL << pin) : (1UL << (pin + 16U));
}

void GPIO_Init_Custom(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;
    (void)RCC->APB2ENR;

    // Dat OFF truoc khi chuyen PA1/PA2 sang output de tranh xung bat.
    Fan_Off();
    Buzzer_Off();

    // Output push-pull, 2 MHz. Giu nguyen PA0 va cac chan khac.
    GPIOA->CRL = (GPIOA->CRL & ~((0xFUL << 4) | (0xFUL << 8))) |
                 (0x2UL << 4) | (0x2UL << 8);

    // PB12/PB13/PB14 input pull-up, nhan nut noi chan xuong GND.
    GPIOB->BSRR = (1UL << 12) | (1UL << 13) | (1UL << 14);
    GPIOB->CRH = (GPIOB->CRH & ~((0xFUL << 16) | (0xFUL << 20) |
                                (0xFUL << 24))) |
                 (0x8UL << 16) | (0x8UL << 20) | (0x8UL << 24);

    for (uint32_t i = 0U; i < BUTTON_COUNT; ++i)
    {
        button_states[i] = (ButtonState){0};
    }
}

void Buzzer_On(void)
{
    GPIO_Write(BUZZER_PIN, BUZZER_ACTIVE_LEVEL);
}

void Buzzer_Off(void)
{
    GPIO_Write(BUZZER_PIN, !BUZZER_ACTIVE_LEVEL);
}

void Fan_On(void)
{
    GPIO_Write(FAN_PIN, FAN_ACTIVE_LEVEL);
}

void Fan_Off(void)
{
    GPIO_Write(FAN_PIN, !FAN_ACTIVE_LEVEL);
}

uint8_t Button_Read(uint8_t button)
{
    if ((button < BUTTON_SELECT) || (button > BUTTON_DOWN))
    {
        return 0U;
    }

    uint32_t pin = 12U + (uint32_t)(button - BUTTON_SELECT);
    return ((GPIOB->IDR & (1UL << pin)) == 0U) ? 1U : 0U;
}

uint8_t Button_ReadDebounced(uint8_t button, uint32_t now_ms)
{
    if ((button < BUTTON_SELECT) || (button > BUTTON_DOWN))
    {
        return 0U;
    }

    ButtonState *state = &button_states[button - BUTTON_SELECT];
    uint8_t raw = Button_Read(button);

    if ((state->initialized == 0U) || (raw != state->last_raw))
    {
        state->last_raw = raw;
        state->changed_at = now_ms;
        state->initialized = 1U;
    }

    if ((uint32_t)(now_ms - state->changed_at) >= BUTTON_DEBOUNCE_MS)
    {
        state->stable = state->last_raw;
    }
    return state->stable;
}

