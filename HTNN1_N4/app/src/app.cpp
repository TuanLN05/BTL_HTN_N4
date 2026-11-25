#include "app.h"
#include "app_ipc.h"
#include "app_log.h"
#if defined(APP_MOCK_UI)
#include "board_i2c.h"
#endif

bool App_Init(void)
{
    if (!App_IPCInit() || !App_LogInit())
        return false;

#if defined(APP_MOCK_UI)
    // Trong ban mock UI, TaskSensor khong chay de khoi tao I2C1.
    (void)Board_I2CInit();
#endif
    return true;
}
