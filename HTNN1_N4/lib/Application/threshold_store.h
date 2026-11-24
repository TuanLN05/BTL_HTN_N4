#ifndef THRESHOLD_STORE_H
#define THRESHOLD_STORE_H
#include "menu.h"
// Chi goi tu task display, khong goi tu ISR.
int ThresholdStore_Load(int32_t values[MENU_LIMIT_COUNT]);
int ThresholdStore_Save(const int32_t values[MENU_LIMIT_COUNT]);
#endif
