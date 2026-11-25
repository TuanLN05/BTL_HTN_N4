#ifndef THRESHOLD_STORE_H
#define THRESHOLD_STORE_H
#include "app_data.h"
#ifdef __cplusplus
extern "C" {
#endif
// Nap truoc khi chay scheduler, luu tu TaskUI.
int ThresholdStore_Load(int32_t values[MENU_LIMIT_COUNT]);
int ThresholdStore_Save(const int32_t values[MENU_LIMIT_COUNT]);
#ifdef __cplusplus
}
#endif
#endif
