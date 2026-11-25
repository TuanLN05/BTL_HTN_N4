#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "stm32f1xx.h"
#include <stdint.h>

#define GPIO_ANALOG        0x0   // Analog
#define GPIO_INPUT         0x4   // Input floating
#define GPIO_INPUT_PULLUP  0x8   // Input pull-up (khong ho tro pull-down)
#define GPIO_OUTPUT_LOW    0x2   // Push-pull 2 MHz, muc ban dau LOW
#define GPIO_OUTPUT_HIGH   0x12  // Push-pull 2 MHz, muc ban dau HIGH
#define GPIO_AF_PP         0xB   // Chuc nang thay the push-pull 50 MHz (vd UART TX)
#define GPIO_AF_OD         0xF   // Chuc nang thay the open-drain 50 MHz (vd I2C SCL/SDA)

/**
 * @brief Cau hinh che do GPIO va muc dau ra ban dau neu co.
 * @param[in] port Cong GPIOA, GPIOB hoac GPIOC.
 * @param[in] pin Chi so chan tu 0 den 15.
 * @param[in] mode Che do GPIO do header nay dinh nghia.
 * @retval 1 Cau hinh thanh cong.
 * @retval 0 Cong, chan hoac che do khong hop le.
 */
uint8_t gpio_Config(GPIO_TypeDef *port, uint8_t pin, uint8_t mode);

/**
 * @brief Doc muc logic tai chan GPIO.
 * @param[in] port Cong GPIO can doc.
 * @param[in] pin Chi so chan tu 0 den 15.
 * @retval 1 Muc HIGH.
 * @retval 0 Muc LOW hoac cong va chan khong hop le.
 */
uint8_t gpio_Read(GPIO_TypeDef *port, uint8_t pin);   // Tra ve 0 hoac 1
/**
 * @brief Dat chan GPIO len muc HIGH bang thanh ghi BSRR.
 * @param[in] port Cong GPIO can ghi.
 * @param[in] pin Chi so chan tu 0 den 15.
 */
void    gpio_Set(GPIO_TypeDef *port, uint8_t pin);    // muc HIGH
/**
 * @brief Dat chan GPIO xuong muc LOW bang thanh ghi BSRR.
 * @param[in] port Cong GPIO can ghi.
 * @param[in] pin Chi so chan tu 0 den 15.
 */
void    gpio_Reset(GPIO_TypeDef *port, uint8_t pin);  // muc LOW

#endif
