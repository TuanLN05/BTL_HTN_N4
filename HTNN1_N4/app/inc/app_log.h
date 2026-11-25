#ifndef APP_LOG_H
#define APP_LOG_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool App_LogInit(void);
// Chi gui khi snprintf khong bi cat. Khoa UART khi scheduler dang chay.
void App_LogLine(const char *text, int length, size_t capacity);

#ifdef __cplusplus
}

template<size_t N>
void App_LogMessage(const char (&text)[N])
{
    App_LogLine(text, N - 1U, N);
}
#else
#define App_LogMessage(message) App_LogLine(message, sizeof(message) - 1U, sizeof(message))
#endif

#endif
