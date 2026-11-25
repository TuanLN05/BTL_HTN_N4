#include "button.h"
#include "board.h"
#include <stddef.h>
#include <string.h>

/**
 * @brief Doc muc nhan cua nut tu BSP.
 * @param[in] index Chi so nut theo thu tu UP, DOWN, SELECT.
 * @param[in] user Ngu canh callback, khong su dung.
 * @retval 1 Nut dang nhan.
 * @retval 0 Nut dang tha hoac chi so sai.
 */
static uint8_t Button_ReadGPIO(uint8_t index, void *user)
{
    (void)user;
    return Board_ButtonRead(index);
}

// Khoi tao GPIO va trang thai chong doi cho ba nut nhan noi xuong GND.
void Button_Init(ButtonManager *b, uint32_t debounce_ms)
{
    if (!b) return;
    Board_ButtonsInit();
    memset(b, 0, sizeof(*b));
    b->read = Button_ReadGPIO;
    b->debounce_ms = debounce_ms ? debounce_ms : 25;
    // Thoi diem ban dau = 0; raw/stable = 0 (chua nhan).
}

ButtonEvent Button_Poll(ButtonManager *b, uint32_t now_ms)
{
    // raw la muc doc tuc thoi; stable chi doi khi raw on dinh du debounce_ms.
    // Can goi dinh ky; ham khong delay hay cho nut duoc tha.
    uint8_t i;
    ButtonEvent result = BUTTON_NONE;
    if (!b || !b->read) return BUTTON_NONE;

    for (i = 0; i < 3; ++i) {
        ButtonKey *k = &b->keys[i];
        uint8_t pressed = b->read(i, b->user) ? 1 : 0;

        if (pressed != k->raw) {
            // Muc chan vua thay doi: bat dau dem lai debounce.
            k->raw = pressed;
            k->changed_at_ms = now_ms;
        }
        if (k->raw != k->stable &&
            (uint32_t)(now_ms - k->changed_at_ms) >= b->debounce_ms) {
            k->stable = k->raw;
            // Phat mot event khi nut duoc nhan on dinh.
            if (k->stable && result == BUTTON_NONE)
                result = (ButtonEvent)(BUTTON_UP + i);
        }
    }
    return result;
}
