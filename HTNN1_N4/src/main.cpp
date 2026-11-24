#include "rtc.h"

DATE rtc_date = {};
TIME rtc_time = {};

int main(void)
{
    i2c1_init();
    rtc_init();

    while (1)
    {
        readRTC(&rtc_date, &rtc_time);
    }
}
