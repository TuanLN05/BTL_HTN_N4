#ifndef HAL_I2C_H
#define HAL_I2C_H

#include "stm32f103xb.h"
#include "stdint.h"

/**
 * @brief Bat clock, cau hinh PB6 va PB7, sau do nap timing co so cho I2C1.
 */
void i2c1_init(void);

/**
 * @brief Phat START va doi co SB.
 * @note Ham polling nay khong co timeout.
 */
void i2c1_Start(void);

/**
 * @brief Yeu cau I2C1 phat STOP.
 */
void i2c1_Stop(void);

/**
 * @brief Gui mot byte va doi den khi truyen xong.
 * @param[in] data Byte can gui.
 * @note Ham polling nay khong co timeout.
 */
void i2c1_writeData(uint8_t data);

/**
 * @brief Doc mot day byte bang giao dich I2C1 polling.
 * @param[in] addr Dia chi I2C 7 bit.
 * @param[out] data Bo dem nhan toi thieu size byte.
 * @param[in] size So byte can doc.
 * @note Ham polling nay khong co timeout.
 */
void i2c1_readBytes(uint8_t addr, uint8_t *data, uint8_t size);

/**
 * @brief Ghi mot day byte bang giao dich I2C1 polling.
 * @param[in] addr Dia chi I2C 7 bit.
 * @param[in] data Bo dem du lieu can ghi.
 * @param[in] size So byte can ghi.
 * @note Ham polling nay khong co timeout.
 */
void i2c1_writeBytes(uint8_t addr, uint8_t *data, uint8_t size);

/**
 * @brief Gui byte dia chi va doi thiet bi xac nhan.
 * @param[in] addr Dia chi da dich trai mot bit va kem bit doc hoac ghi.
 * @note Ham polling nay khong co timeout.
 */
void i2c1_writeAddr(uint8_t addr);

#endif
