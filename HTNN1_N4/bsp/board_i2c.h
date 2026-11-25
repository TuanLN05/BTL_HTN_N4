#ifndef BOARD_I2C_H
#define BOARD_I2C_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Phuc hoi I2C1 bang xung SCL, dieu kien STOP va reset ngoai vi.
 * @pre TIM2 da chay voi buoc dem 1 ms.
 * @note Trong task phai giu mutex I2C1.
 */
void Board_I2CRecoverBus(void);

/**
 * @brief Khoi tao I2C1 o 80 kHz va kiem tra bus da ranh.
 * @retval 0 Bus san sang.
 * @retval -1 Clock APB1 khong hop le hoac bus van bi giu.
 * @pre TIM2 da duoc khoi tao. Trong task phai giu mutex I2C1.
 */
int Board_I2CInit(void);

/**
 * @brief Ghi mot day byte toi thiet bi tren I2C1 co kiem tra loi va timeout.
 * @param[in] address Dia chi I2C 7 bit.
 * @param[in] data Bo dem can gui.
 * @param[in] size So byte can ghi, lon hon 0.
 * @retval true Giao dich thanh cong.
 * @retval false Dau vao sai, NACK, loi bus hoac het thoi gian cho.
 * @pre Task goi dang giu mutex I2C1.
 */
bool Board_I2CWrite(uint8_t address, const uint8_t *data, uint16_t size);

/**
 * @brief Doc mot day byte tu thiet bi tren I2C1 co kiem tra loi va timeout.
 * @param[in] address Dia chi I2C 7 bit.
 * @param[out] data Bo dem nhan toi thieu size byte.
 * @param[in] size So byte can doc, lon hon 0.
 * @retval true Giao dich thanh cong.
 * @retval false Dau vao sai, NACK, loi bus hoac het thoi gian cho.
 * @pre Task goi dang giu mutex I2C1.
 */
bool Board_I2CRead(uint8_t address, uint8_t *data, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif
