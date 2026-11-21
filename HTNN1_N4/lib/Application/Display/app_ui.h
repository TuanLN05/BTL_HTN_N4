#ifndef APP_UI_H
#define APP_UI_H
#include "menu.h"

// Khoi tao va cap nhat giao dien trong task hien thi.
int App_UI_Init(void);
int App_UI_Poll(uint32_t now_ms);
Menu *App_UI_GetMenu(void);

#endif