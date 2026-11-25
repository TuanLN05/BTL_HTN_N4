#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

// 3 nut: UP / DOWN / SELECT. Khong co thao tac nhan giu.
typedef enum {
    BUTTON_NONE = 0,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_SELECT
} ButtonEvent;

/**
 * @brief Callback doc trang thai vat ly cua mot nut nhan.
 * @param[in] button_index Chi so nut theo thu tu UP, DOWN, SELECT.
 * @param[in] user Du lieu ngu canh cua callback.
 * @retval 1 Nut dang duoc nhan.
 * @retval 0 Nut dang tha.
 */
typedef uint8_t (*ButtonReadFn)(uint8_t button_index, void *user);

typedef struct {
    uint8_t raw;       // trang thai moi doc
    uint8_t stable;    // trang thai da qua loc nhieu
    uint32_t changed_at_ms;
} ButtonKey;

typedef struct {
    ButtonReadFn read;
    void *user;
    ButtonKey keys[3];
    uint32_t debounce_ms;
} ButtonManager;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khoi tao GPIO qua BSP va dat lai trang thai chong doi.
 * @param[out] b Bo quan ly nut nhan.
 * @param[in] debounce_ms Thoi gian on dinh tinh bang ms. Dung 0 de chon 25 ms.
 */
void Button_Init(ButtonManager *b, uint32_t debounce_ms);

/**
 * @brief Quet nut nhan va phat su kien khi muc nhan on dinh.
 * @param[in,out] b Bo quan ly nut nhan.
 * @param[in] now_ms Moc thoi gian hien tai tinh bang ms.
 * @return Su kien cua mot nut vua nhan, hoac BUTTON_NONE neu khong co.
 */
ButtonEvent Button_Poll(ButtonManager *b, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
