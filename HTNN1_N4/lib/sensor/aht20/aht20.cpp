#include "aht20.h"
void delay(uint32_t time)
{
	while(time)
	{
		SysTick -> LOAD = 72000 - 1;
		SysTick -> CTRL = 5;
		SysTick -> VAL = 0;
		while(!(SysTick -> CTRL & (1 << 16)));
		time--;
	}
} 
void aht20_init()
{
	delay(100);
	uint8_t cmd[3] = {0xBE, 0x08, 0x00};
	i2c1_writeBytes(aht20_addr, cmd, 3);
	delay(10);
}

void aht20_reset()
{
	  uint8_t cmd = AHT20_SOFTRESET;
    i2c1_writeBytes(aht20_addr, &cmd, 1);
    delay(20);
}
void readAHT20(float *temp, float *humi)
{
	uint8_t cmd[3] = {0xAC, 0x33, 0x00};
	uint8_t data[7];
	//i2c1_Start();
	i2c1_writeBytes(aht20_addr, cmd, 3);
	delay(80);
	
	i2c1_readBytes(aht20_addr, data, 7);
	
	if(data[0] & 0x80)
	{
		delay(10);
		return;
	}
	uint32_t raw_h = (uint32_t)data[1] << 12 | (uint32_t)data[2] << 4 | (uint32_t)data[3] >> 4;
	uint32_t raw_t = (((uint32_t)data[3] & 0x0F) << 16) | (uint32_t)data[4] << 8 | (uint32_t)data[5];
	*temp = ((raw_t * 200.0)/ 1048576.0) - 50.0;
	*humi = (raw_h * 100.0) / 1048576.0;
}
float readAHT20_temp()
{
    float temp, humi;
    readAHT20(&temp, &humi);
    return temp;
}
float readAHT20_humi()
{
    float temp, humi;
    readAHT20(&temp, &humi);
    return humi;
}