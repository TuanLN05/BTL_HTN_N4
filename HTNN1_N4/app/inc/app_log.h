#ifndef APP_LOG_H
#define APP_LOG_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tao mutex bao ve log UART giua cac task.
 * @retval true Da tao mutex.
 * @retval false Khong du bo nho.
 */
bool App_LogInit(void);

/**
 * @brief Gui dong log UART neu do dai nam trong bo dem.
 * @param[in] text Bo dem ky tu can gui.
 * @param[in] length So byte muon gui, thuong la ket qua snprintf().
 * @param[in] capacity Dung luong bo dem tinh theo byte.
 * @note Khoa UART khi scheduler dang chay.
 */
void App_LogLine(const char *text, int length, size_t capacity);

#ifdef __cplusplus
}

/**
 * @brief Gui chuoi log UART va bo byte ket thuc chuoi.
 * @tparam N Kich thuoc mang ky tu gom byte ket thuc.
 * @param[in] text Mang ky tu chua thong diep.
 */
template<size_t N>
void App_LogMessage(const char (&text)[N])
{
    App_LogLine(text, N - 1U, N);
}
#else
#define App_LogMessage(message) App_LogLine(message, sizeof(message) - 1U, sizeof(message))
#endif

#endif
