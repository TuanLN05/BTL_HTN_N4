#ifndef TASK_DISPLAY_H
#define TASK_DISPLAY_H
#include <stdint.h>

// 0: chua vao task; 1: cho nguon; 2: dang init; 3: init loi; 4: dang chay.
extern volatile uint8_t g_oled_stage;
extern volatile uint32_t g_oled_init_attempts;
extern volatile int g_oled_last_i2c_error;
extern volatile int g_oled_render_result;

void Task_Display(void *argument);
#endif