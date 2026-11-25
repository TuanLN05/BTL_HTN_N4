#ifndef modeBoot_h
#define modeBoot_h

#include <Arduino.h>
#define bootEsp32       0
#define STM32_BOOT0     25
#define STM32_NRST      26




void modeBootInit();
bool modeBoot();
void modeApplication();

#endif
