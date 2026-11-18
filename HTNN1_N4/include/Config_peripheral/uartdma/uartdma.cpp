#include "uartdma.h"
#include "stm32f103xb.h"

static bool initialized = false;

bool dma_uart(uint32_t baudrate)
{
    if (baudrate == 0U) return false;

    RCC->AHBENR |= (1U << 0);                    // DMA1EN.
    RCC->APB2ENR |= (1U << 2) | (1U << 14);     // IOPAEN, USART1EN.
    (void)RCC->AHBENR;
    (void)RCC->APB2ENR;

    // CR2/CR3 giu mac dinh; tat USART de co the cau hinh lai.
    USART1->CR1 &= ~(1U << 13); // UE: tat USART1.
    GPIOA->ODR |= (1U << 10);
    GPIOA->CRH = (GPIOA->CRH & ~((0xFU << 4) | (0xFU << 8))) |
                 (0xBU << 4) |  // PA9: TX, alternate function push-pull.
                 (0x8U << 8);   // PA10: RX, input pull-up.
    USART1->BRR = 8000000U / baudrate; // Clock APB2 mac dinh 8 MHz.
    (void)USART1->SR;
    (void)USART1->DR;

    // Channel 4: RAM -> USART1; Channel 5: USART1 -> RAM, moi lan 1 byte.
    DMA1_Channel4->CCR &= ~(1U << 0); // EN: tat Channel 4.
    DMA1_Channel5->CCR &= ~(1U << 0); // EN: tat Channel 5.
    DMA1->IFCR = (1U << 12) | (1U << 16); // CGIF4, CGIF5: xoa co DMA.
    DMA1_Channel4->CPAR = (uint32_t)&USART1->DR;
    DMA1_Channel5->CPAR = (uint32_t)&USART1->DR;
    DMA1_Channel4->CCR = (1U << 7) | (1U << 4); // MINC, DIR: RAM -> USART1.
    DMA1_Channel5->CCR = (1U << 7);             // MINC: tang dia chi RAM.

    USART1->CR1 = (1U << 13) | (1U << 3) | (1U << 2); // UE, TE, RE.
    initialized = true;
    return true;
}

bool dma_uart_read(uint8_t *buffer, uint16_t size)
{
    if (!initialized || buffer == nullptr || size == 0U ||
        (DMA1_Channel5->CCR & (1U << 0)) != 0U) return false;

    // SR: ORE bit 3, FE bit 1, NE bit 2, PE bit 0.
    const uint32_t rx_errors = (1U << 3) | (1U << 1) | (1U << 2) | (1U << 0);
    DMA1->IFCR = (1U << 16); // CGIF5.
    DMA1_Channel5->CMAR = (uint32_t)buffer;
    DMA1_Channel5->CNDTR = size;
    __DMB();
    DMA1_Channel5->CCR |= (1U << 0); // EN.
    USART1->CR3 |= (1U << 6);        // DMAR: bat yeu cau DMA RX.

    // Cho nhan du size byte, hoac dung khi DMA/USART bao loi.
    // ISR: TCIF5 bit 17, TEIF5 bit 19.
    while ((DMA1->ISR & ((1U << 17) | (1U << 19))) == 0U &&
           (USART1->SR & rx_errors) == 0U) {}

    USART1->CR3 &= ~(1U << 6);        // DMAR.
    DMA1_Channel5->CCR &= ~(1U << 0); // EN.
    bool success = (DMA1->ISR & (1U << 17)) != 0U &&
                   (DMA1->ISR & (1U << 19)) == 0U &&
                   (USART1->SR & rx_errors) == 0U;
    if (!success) {
        // Doc SR roi DR de xoa loi, cho phep lan nhan sau hoat dong.
        (void)USART1->SR;
        (void)USART1->DR;
    }
    DMA1->IFCR = (1U << 16); // CGIF5.
    __DMB();
    return success;
}

bool dma_uart_writebytes(const uint8_t *buffer, uint16_t size)
{
    if (!initialized || buffer == nullptr || size == 0U ||
        (DMA1_Channel4->CCR & (1U << 0)) != 0U) return false;

    DMA1->IFCR = (1U << 12); // CGIF4.
    DMA1_Channel4->CMAR = (uint32_t)buffer;
    DMA1_Channel4->CNDTR = size;
    USART1->SR = ~(1U << 6); // TC: xoa co truyen xong.
    __DMB();
    DMA1_Channel4->CCR |= (1U << 0); // EN.
    USART1->CR3 |= (1U << 7);        // DMAT: bat yeu cau DMA TX.

    // Cho DMA dua het du lieu vao USART1.
    // ISR: TCIF4 bit 13, TEIF4 bit 15.
    while ((DMA1->ISR & ((1U << 13) | (1U << 15))) == 0U) {}

    USART1->CR3 &= ~(1U << 7);        // DMAT.
    DMA1_Channel4->CCR &= ~(1U << 0); // EN.
    bool success = (DMA1->ISR & (1U << 15)) == 0U; // TEIF4.
    DMA1->IFCR = (1U << 12); // CGIF4.
    if (success) {
        // DMA xong chua co nghia la bit cuoi da ra chan TX.
        while ((USART1->SR & (1U << 6)) == 0U) {} // TC.
    }
    return success;
}
