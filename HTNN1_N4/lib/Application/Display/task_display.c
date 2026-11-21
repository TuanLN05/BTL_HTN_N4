#include "task_display.h"
#include "app_ui.h"
#include "app_ipc.h"
#include "menu.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stddef.h>

// Cac bien nay de xem trong debugger: dang o buoc nao, so lan init va loi I2C.
volatile uint8_t g_oled_stage = 0;
volatile uint32_t g_oled_init_attempts = 0;
volatile int g_oled_last_i2c_error = 0;
volatile int g_oled_render_result = 0;

// Tao bang xTaskCreate; khong goi truc tiep vi ham chay mai trong vong lap.
// main phai khoi tao phan cung, mutex I2C va cac queue truoc khi task chay.
void Task_Display(void *argument)
{
    DisplayMessage msg;
    Menu *menu;
    TickType_t last;
    (void)argument;

    // Chinh FreeRTOS xu ly thoi gian cho nguon OLED on dinh.
    g_oled_stage = 1;
    vTaskDelay(pdMS_TO_TICKS(100));

    // UI_Init goi SH1106_Init qua I2C_RTOS_Write.
    // Neu I2C loi, khong khoa CPU: cho va thu lai.
    for (;;) {
        g_oled_stage = 2;
        ++g_oled_init_attempts;
        if (App_UI_Init() == 0) break;
        g_oled_stage = 3;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    g_oled_stage = 4;

    menu = App_UI_GetMenu();
    last = xTaskGetTickCount();

    for (;;)
    {
        // Lay cac mau tu task cam bien/RTC.
        // CHI OLED Task duoc phep sua Menu -> khong race condition.
        // Timeout 0: queue rong thi chuyen ngay sang quet nut va ve man hinh.
        while (xQueueReceive(g_display_queue, &msg, 0) == pdTRUE)
        {
            if (msg.kind == DISPLAY_MSG_SENSOR)
                Menu_SetSensor(menu, msg.sensor, msg.value, msg.valid);
            else if (msg.kind == DISPLAY_MSG_RTC)
                Menu_SetRTC(menu, &msg.rtc);
        }

        // Loc 3 nut, xu ly menu, ve lai OLED neu noi dung thay doi.
        // Co su dung xTaskGetTickCount de tinh debounce 25ms.
        g_oled_render_result = App_UI_Poll((uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS));

        // Chu ky quet 20ms, KHONG dung I2C_DelayMs().
        vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
    }
}
