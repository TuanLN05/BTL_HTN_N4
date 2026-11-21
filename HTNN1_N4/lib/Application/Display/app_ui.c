#include "threshold_store.h"
#include "app_ipc.h"
#include "app_ui.h"
#include "board_port.h"
#include "button.h"
#include "sh1106.h"

// Giu trang thai UI suot chuong trinh; cac doi tuong nay do task display su dung.
static Sh1106 oled;
static ButtonManager buttons;
static Menu menu;

// Khoi tao OLED, nut bam, menu va nap nguong da luu trong Flash; tra -1 neu OLED loi.
int App_UI_Init(void)
{
    // Hook noi menu voi board: menu chi bao su kien, board gui yeu cau qua queue.
    int32_t saved[MENU_LIMIT_COUNT];
    unsigned i;
    MenuHooks hooks = {0};
    hooks.threshold_saved = Board_OnThresholdChanged;
    hooks.rtc_write = Board_WriteRTC;
    hooks.user = 0;

    // SH1106_Init chi gui lenh dieu khien SH1106 qua callback I2C.
    if (SH1106_Init(&oled, Board_I2CWrite, 0, SH1106_ADDR7, Board_DelayMs) != 0)
        return -1;
    Button_Init(&buttons, Board_ReadButton, 0, 25);
    Menu_Init(&menu, &oled, &hooks);
    // Flash trong/loi thi giu nguong mac dinh.
    if (ThresholdStore_Load(saved) == 0) {
        for (i = 0; i < MENU_LIMIT_COUNT; ++i)
            Menu_SetThreshold(&menu, (MenuLimit)i, saved[i]);
    }
    for (i = 0; i < MENU_LIMIT_COUNT; ++i)
        (void)Display_RequestLimit((MenuLimit)i, Menu_GetThreshold(&menu, (MenuLimit)i));
    return 0;
}

// Quet va loc nut bam, xu ly su kien menu, sau do ve lai neu noi dung thay doi.
int App_UI_Poll(uint32_t now_ms)
{
    // Moi luot: loc nut -> doi trang/gia tri menu -> ve neu co thay doi.
    ButtonEvent event = Button_Poll(&buttons, now_ms);
    if (event != BUTTON_NONE) Menu_HandleButton(&menu, event);
    return Menu_Render(&menu); // 0 khong can ve, 1 da ve, -1 I2C loi
}

Menu *App_UI_GetMenu(void)
{
    // Task display lay doi tuong nay de cap nhat du lieu nhan tu queue.
    return &menu;
}
