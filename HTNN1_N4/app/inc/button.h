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

// Callback doc nut: tra ve 1 neu DANG NHAN, 0 neu KHONG nhan.
// Callback chuyen muc chan GPIO thanh trang thai nhan nut.
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

// Khoi tao GPIO qua BSP va chong doi; anh xa chan trong bsp/board_pins.h.
// debounce_ms = 0 thi dung mac dinh 25 ms.
void Button_Init(ButtonManager *b, uint32_t debounce_ms);

// Goi dinh ky (vd 10ms). Khi NHAN nut on dinh, tao 1 event.
// Nhung nut duoc giu khong bi lap su kien lien tuc.
ButtonEvent Button_Poll(ButtonManager *b, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
