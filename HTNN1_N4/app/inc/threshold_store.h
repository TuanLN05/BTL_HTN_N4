#ifndef THRESHOLD_STORE_H
#define THRESHOLD_STORE_H
#include "app_data.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Nap bo nguong tu ban ghi Flash hop le moi nhat.
 * @param[out] values Mang MENU_LIMIT_COUNT phan tu.
 * @retval 0 Nap thanh cong.
 * @retval -1 Khong co ban ghi hop le hoac con tro dau ra rong.
 */
int ThresholdStore_Load(int32_t values[MENU_LIMIT_COUNT]);

/**
 * @brief Luu bo nguong luan phien tren hai page Flash va ghi dau commit cuoi cung.
 * @param[in] values Mang MENU_LIMIT_COUNT nguong can luu.
 * @retval 0 Da luu thanh cong hoac du lieu khong doi.
 * @retval -1 Nguong khong hop le hoac thao tac Flash that bai.
 * @note Goi tu TaskUI.
 */
int ThresholdStore_Save(const int32_t values[MENU_LIMIT_COUNT]);
#ifdef __cplusplus
}
#endif
#endif
