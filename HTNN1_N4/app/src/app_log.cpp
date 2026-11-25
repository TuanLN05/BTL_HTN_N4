#include "app_log.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "hal_uart.h"

static SemaphoreHandle_t uart_mutex;

bool App_LogInit(void)
{
    uart_mutex = xSemaphoreCreateMutex();
    return uart_mutex != nullptr;
}

void App_LogLine(const char *text, int length, size_t capacity)
{
    if (length <= 0 || static_cast<size_t>(length) >= capacity)
        return;
    const bool running = xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
    if (running)
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
    dma_uart_writebytes(reinterpret_cast<const uint8_t *>(text), static_cast<uint16_t>(length));
    if (running)
        xSemaphoreGive(uart_mutex);
}
