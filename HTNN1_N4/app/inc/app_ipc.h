#ifndef APP_IPC_H
#define APP_IPC_H

#include "FreeRTOS.h"
#include "app_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Nap nguong tu Flash neu co va tao hai queue du lieu cung mutex I2C1.
 * @retval true Ca hai queue va mutex da duoc tao.
 * @retval false Khong du bo nho de tao tai nguyen IPC.
 */
bool App_IPCInit(void);

/**
 * @brief Sao chep mot mau vao queue TaskSensor den TaskAlarm.
 * @param[in] data Mau can gui.
 * @note Task goi se cho neu queue day.
 */
void App_IPCSendSensor(const SensorData_t *data);

/**
 * @brief Cho va lay mot mau khoi queue cam bien.
 * @param[out] data Noi luu mau nhan duoc.
 */
void App_IPCReceiveSensor(SensorData_t *data);

/**
 * @brief Ghi de queue mot phan tu cua TaskUI bang trang thai moi nhat.
 * @param[in] data Mau cam bien kem mat na canh bao.
 */
void App_IPCPublishAlarm(const AlarmData_t *data);

/**
 * @brief Thu lay trang thai canh bao moi nhat ma khong cho.
 * @param[out] data Noi luu mau neu co du lieu.
 * @retval true Da lay mot mau.
 * @retval false Queue dang rong.
 */
bool App_IPCReceiveAlarm(AlarmData_t *data);

/**
 * @brief Sao chep bo nguong hien tai trong vung toi han FreeRTOS.
 * @param[out] values Mang MENU_LIMIT_COUNT phan tu theo thu tu MenuLimit.
 */
void App_GetThresholds(int32_t values[MENU_LIMIT_COUNT]);

/**
 * @brief Cap nhat ca bo nguong trong RAM trong mot vung toi han.
 * @param[in] values Mang MENU_LIMIT_COUNT phan tu theo thu tu MenuLimit.
 */
void App_SetThresholds(const int32_t values[MENU_LIMIT_COUNT]);

/**
 * @brief Thu giu mutex cua bus I2C1.
 * @param[in] timeout Thoi gian cho tinh theo tick. Dung 0 de thu ngay.
 * @retval true Da giu mutex.
 * @retval false Khong lay duoc mutex trong thoi gian cho.
 */
bool App_I2CTake(TickType_t timeout);

/**
 * @brief Nha mutex I2C1 do task hien tai dang giu.
 */
void App_I2CGive(void);

#ifdef __cplusplus
}
#endif

#endif
