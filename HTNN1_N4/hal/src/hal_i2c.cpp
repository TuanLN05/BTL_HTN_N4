#include "hal_i2c.h"

void i2c1_init(void)
{
	RCC -> APB1ENR |= 1 << 21; 
	RCC -> APB2ENR |= 1 << 3; 
	
	GPIOB -> CRL &= ~(0xFF << 6 * 4);
	GPIOB -> CRL |= 0xFF << 6 * 4; // PB6 va PB7
	
	// cai dat baud rate 
	I2C1 -> CR2 = 16 ; // 0x001000 -> 16MHz -> fPCLK = 16MHz
	// khoi tao baud rate cho master 
	// standard mode = 100kHz; fast mode = 400kHz
	I2C1 -> CCR = 80; // CCR = CR2 / baud * 2 
	// cau hinh thoi gian canh len toi da
	I2C1 -> TRISE |= 17; // Trise = 1000ns / (1/CR2) + 1
	I2C1 -> CR1 |= 1 << 0; // enable I2C
}

void i2c1_Start(void)
{
	I2C1 -> CR1 |= 1 << 10; // bit ACK returned sau khi nhan byte
	I2C1 -> CR1 |= 1 << 8; // start generaion 
	while(!(I2C1 -> SR1 & (1 << 0))); // doi bit Start Bit = 1 -> start da duoc gui
}

void i2c1_Stop(void)
{
	I2C1 -> CR1 |= 1 << 9;
}

void i2c1_writeAddr(uint8_t addr)
{
	I2C1 -> DR = addr;
	while(!(I2C1 -> SR1 & (1 << 1))); // cho khi bit ADDR = 1 -> gui dia chi thanh cong
	(void)I2C1 -> SR2; // clear ADDR
}

void i2c1_writeData(uint8_t data)
{
	while(!(I2C1 -> SR1 & (1 << 7))); // cho cho den khi thanh ghi DR chong
	I2C1 -> DR = data; // copy data vao DR
	while(!(I2C1 -> SR1 & (1 << 2))); // cho cho den khi truyen thanh cong
}
void i2c1_writeBytes(uint8_t addr, uint8_t *data, uint8_t size)
{
	i2c1_Start();
	i2c1_writeAddr(addr << 1); // W = 0. R = 1
	for(int i = 0; i < size; i++)
	{
		i2c1_writeData(data[i]);
	}
	i2c1_Stop();
}

void i2c1_readBytes(uint8_t addr, uint8_t *data, uint8_t size)
{
	i2c1_Start();
	i2c1_writeAddr((addr << 1 )| 1); // chen bit 1 (read) vao ben trai addr
	for(int i = 0; i < size; i++)
	{
		if(i == size - 1)
		{
			I2C1 -> CR1 &= ~(1 << 10); // gui NACK cho byte cuoi
		}
		while(!(I2C1 -> SR1 & (1 << 6)));// doi bit RXNE
		data[i] = I2C1 -> DR;
	}
	i2c1_Stop();
}
