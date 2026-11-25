#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdbool.h>
#include <stdint.h>



/**
 * @brief Cau hinh USART1 8N1 va DMA voi PA9 TX, PA10 RX.
 * @param[in] baudrate Toc do baud can dat. Gia tri 0 se bo qua cau hinh.
 * @pre APB2 o 8 MHz va khong co giao dich UART dang chay.
 * @note Dung DMA1 Channel 4 cho TX va Channel 5 cho RX.
 */
void dma_uart(uint32_t baudrate);

/**
 * @brief Nhan du so byte yeu cau qua USART1 DMA polling.
 * @param[out] buffer Bo dem nhan toi thieu size byte.
 * @param[in] size So byte can nhan.
 * @retval true Da nhan du byte.
 * @retval false Chua khoi tao, tham so sai, kenh ban hoac co loi.
 * @note Khong co timeout va khong them byte ket thuc chuoi.
 */
bool dma_uart_read(uint8_t *buffer, uint16_t size);

/**
 * @brief Gui cac byte qua DMA va doi den khi USART1 gui xong bit cuoi.
 * @param[in] buffer Bo dem chua du lieu can gui.
 * @param[in] size So byte can gui.
 * @retval true Da gui xong.
 * @retval false Chua khoi tao, tham so sai, kenh ban hoac DMA loi.
 * @note Goi tuan tu va khong goi tu ISR. Ham khong co timeout.
 */
bool dma_uart_writebytes(const uint8_t *buffer, uint16_t size);



#endif
