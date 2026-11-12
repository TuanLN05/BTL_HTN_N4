#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

#define BUTTON_SELECT 1U
#define BUTTON_UP     2U
#define BUTTON_DOWN   3U

#ifndef FAN_ACTIVE_LEVEL
#define FAN_ACTIVE_LEVEL 1U
#endif
#ifndef BUZZER_ACTIVE_LEVEL
#define BUZZER_ACTIVE_LEVEL 1U
#endif

#if (FAN_ACTIVE_LEVEL > 1U) || (BUZZER_ACTIVE_LEVEL > 1U)
#error "FAN_ACTIVE_LEVEL and BUZZER_ACTIVE_LEVEL must be 0 or 1"
#endif

#define BUTTON_DEBOUNCE_MS 20U

// PA1: relay
//PA2: coi
//PB12/PB13/PB14: SELECT/UP/DOWN, nut noi GND.
void GPIO_Init_Custom(void);
void Buzzer_On(void);
void Buzzer_Off(void);
void Fan_On(void);
void Fan_Off(void);

// Muc tuc thoi: 1 = nhan, 0 = nha hoac ma nut khong hop le.
uint8_t Button_Read(uint8_t button);

// Goi deu (vi du moi 1 ms) tu mot task; now_ms la bo dem ms tang don dieu.
uint8_t Button_ReadDebounced(uint8_t button, uint32_t now_ms);

#endif
