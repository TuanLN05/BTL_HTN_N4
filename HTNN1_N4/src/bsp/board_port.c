#include "threshold_store.h"
#include "app_ui.h"
#include "board_port.h"
#include "task_display.h"
#include "FreeRTOS.h"
#include "task.h"
#include "GPIO.h"         
#include "i2c_rtos.h"     
#include "app_ipc.h"     

// Lop noi thu vien UI voi phan cung va FreeRTOS cua board dang dung.
// Callback delay nay chi dung khi scheduler da chay (khoi tao OLED trong task).
void Board_DelayMs(uint32_t ms, void *user)
{
    (void)user;
    vTaskDelay(pdMS_TO_TICKS(ms) + 1);
}

void Board_InitButtons(void)
{
    // Nut noi PB12/PB13/PB14
    (void)gpio_Config(GPIOB, 12, GPIO_INPUT_PULLUP);
    (void)gpio_Config(GPIOB, 13, GPIO_INPUT_PULLUP);
    (void)gpio_Config(GPIOB, 14, GPIO_INPUT_PULLUP);
}

uint8_t Board_ReadButton(uint8_t index, void *user)
{
    // Thu tu callback: UP, DOWN, SELECT.
    const uint8_t pins[3] = {12, 14, 13};
    (void)user;
    if (index >= 3) return 0;
    return gpio_Read(GPIOB, pins[index]) == 0;
}

int Board_I2CWrite(uint8_t addr7, const uint8_t *data,
                   uint16_t len, void *user)
{
    (void)user;
    // Truyen dia chi 7-bit; driver I2C tu them bit R/W.
    g_oled_last_i2c_error = (int)I2C_RTOS_Write(addr7, data, len);
    return g_oled_last_i2c_error;
}

int Board_WriteRTC(const MenuDateTime *t, void *user)
{
    (void)user;
    // 0 = QUEUE nhan duoc YEU CAU. RTC Task phai xu ly ghi that.
    return Display_RequestRTC(t) == pdTRUE ? 0 : -1;
}

int Board_OnThresholdChanged(MenuLimit id, int32_t value, void *user)
{
    int32_t values[MENU_LIMIT_COUNT];
    Menu *m = App_UI_GetMenu();
    unsigned i;
    (void)user;
    if ((unsigned)id >= MENU_LIMIT_COUNT) return -1;
    for (i = 0; i < MENU_LIMIT_COUNT; ++i)
        values[i] = Menu_GetThreshold(m, (MenuLimit)i);
    values[id] = value;
    if (ThresholdStore_Save(values) != 0) return -1;
    // Luu thanh cong moi thong bao nguong moi cho task dieu khien.
    (void)Display_RequestLimit(id, value);
    return 0;
}
