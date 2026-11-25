#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include <stdbool.h>

#define BOARD_UART_BAUD 115200U

/**
 * @brief Cap nhat clock va khoi tao NVIC, LED PA1, USART1 va TIM2 truoc khi tao task.
 * @note LED PA1 bat dau o muc LOW. TIM2 dem voi buoc 1 ms.
 */
void Board_Init(void);

#ifdef __cplusplus
extern "C"
#endif

/**
 * @brief Dat trang thai LED canh bao tai PA1.
 * @param[in] on true de xuat HIGH, false de xuat LOW.
 */
void Board_SetAlarmLed(bool on);

/**
 * @brief Cau hinh cac chan UP, DOWN va SELECT lam dau vao pull-up.
 */
void Board_ButtonsInit(void);

/**
 * @brief Doc mot nut nhan active-low.
 * @param[in] index Chi so 0, 1 hoac 2 tuong ung UP, DOWN hoac SELECT.
 * @retval 1 Nut dang duoc nhan.
 * @retval 0 Nut dang tha hoac chi so khong hop le.
 */
uint8_t Board_ButtonRead(uint8_t index);

#endif
