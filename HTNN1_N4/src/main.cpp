#include "stm32f103xb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "i2c_rtos.h"
#include "gpio.h"
#include "uartdma.h"
#include "FreeRTOSConfig.h"
#include "ADC.h"
#include "aht20_bmp280.h"
// CMSIS startup calls main; keep setup/loop as application hooks.
int main(void)
{
    while (1)
    {

    }
}
