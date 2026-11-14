#ifndef UARTDMA_H
#define UARTDMA_H

#include <stdbool.h>
#include <stdint.h>



/* USART1: PA9 TX, PA10 RX, 8N1; DMA1 Channel 4 TX, Channel 5 RX.
 * Dung clock mac dinh HSI 8 MHz, AHB/APB2 khong chia.
 * Goi dma_uart() truoc khi doc/ghi; giu clock APB2 o 8 MHz.
 * Chi cau hinh khi khong truyen/nhan; dung anh xa chan mac dinh.
 * Ca 3 ham tra true khi thanh cong, false khi tham so sai/kenh ban/loi.
 */
bool dma_uart(uint32_t baudrate);

/* Blocking, khong timeout: cho du size byte; khong them ky tu '\0'.
 * Chi dung buffer khi tra true. RX chi hoat dong trong luc goi ham.
 */
bool dma_uart_read(uint8_t *buffer, uint16_t size);

/* Blocking, khong timeout: cho gui xong ca bit cuoi tren TX.
 * Goi cac ham tuan tu trong main, khong goi tu ISR/nhieu tac vu.
 */
bool dma_uart_writebytes(const uint8_t *buffer, uint16_t size);



#endif
