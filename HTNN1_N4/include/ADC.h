#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#define ADC_MAX_VALUE 4095U
#define ADC_READ_ERROR UINT16_MAX
#define ADC_MV_ERROR UINT32_MAX

#ifndef ADC_VREF_MV
#define ADC_VREF_MV 3300U
#endif

typedef enum
{
    ADC_STATUS_OK = 0,
    ADC_STATUS_TIMEOUT
} ADC_Status;

// Khoi tao ADC1 channel 0 (PA0);
ADC_Status ADC_Init_Custom(void);

// Tra ve 0..4095; ADC_READ_ERROR neu chua khoi tao hoac timeout.
// Chi mot task duoc su dung ADC1 tai mot thoi diem.
uint16_t ADC_Read_MQ5(void);

// Dien ap tai PA0, khong phai nong do ppm. Dau vao loi tra ADC_MV_ERROR.
uint32_t ADC_To_mV(uint16_t adc_value);

#endif
