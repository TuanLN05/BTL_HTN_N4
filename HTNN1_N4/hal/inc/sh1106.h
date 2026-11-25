#ifndef SH1106_H
#define SH1106_H

#include <stdint.h>

// Driver cho module SH1106 128x64, giao tiep I2C.
#define SH1106_WIDTH   128
#define SH1106_HEIGHT   64
#define SH1106_ADDR7  0x3C // dia chi I2C 7-bit

// Tham so data[0] da la byte dieu khien 0x00/0x40 cua SH1106.
// Return: 0 = I2C thanh cong; khac 0 = loi I2C.

/**
 * @brief Callback gui lenh hoac du lieu SH1106 qua I2C.
 * @param[in] addr7 Dia chi I2C 7 bit.
 * @param[in] data Goi du lieu bat dau bang byte dieu khien 0x00 hoac 0x40.
 * @param[in] length Do dai goi tinh theo byte.
 * @param[in] user Du lieu ngu canh cua callback.
 * @retval 0 Ghi thanh cong.
 * @return Ma loi khac 0 neu ghi that bai.
 */
typedef int (*Sh1106I2CWrite)(uint8_t addr7,
                              const uint8_t *data,
                              uint16_t length,
                              void *user);

typedef struct {
    Sh1106I2CWrite write;
    void *user;
    uint8_t address7;
    uint8_t framebuffer[SH1106_WIDTH * SH1106_HEIGHT / 8];
    int last_error;
} Sh1106;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Cau hinh SH1106 va giu man hinh tat den khi co khung dau tien.
 * @param[out] d Doi tuong OLED duoc khoi tao.
 * @param[in] write_fn Callback gui giao dich I2C.
 * @param[in] user Du lieu ngu canh truyen cho callback.
 * @param[in] address7 Dia chi I2C 7 bit cua OLED.
 * @retval 0 Cau hinh thanh cong.
 * @return Ma loi khac 0 neu tham so sai hoac loi I2C.
 */
int SH1106_Configure(Sh1106 *d, Sh1106I2CWrite write_fn, void *user, uint8_t address7);

/**
 * @brief Gui lenh bat man hinh SH1106.
 * @param[in,out] d Doi tuong OLED da cau hinh.
 * @retval 0 Gui lenh thanh cong.
 * @return Ma loi khac 0 neu khong gui duoc lenh.
 */
int SH1106_PowerOn(Sh1106 *d);

/**
 * @brief Xoa cac diem anh trong framebuffer.
 * @param[in,out] d Doi tuong OLED chua framebuffer.
 */
void SH1106_Clear(Sh1106 *d);

/**
 * @brief Bat hoac tat mot diem anh trong framebuffer.
 * @param[in,out] d Doi tuong OLED chua framebuffer.
 * @param[in] x Toa do cot tu 0 den 127.
 * @param[in] y Toa do hang tu 0 den 63.
 * @param[in] on Khac 0 de bat, 0 de tat.
 */
void SH1106_DrawPixel(Sh1106 *d, uint8_t x, uint8_t y, uint8_t on);

/**
 * @brief Ve chuoi ky tu font 5x7 vao framebuffer.
 * @param[in,out] d Doi tuong OLED chua framebuffer.
 * @param[in] x Cot bat dau.
 * @param[in] y Hang bat dau.
 * @param[in] s Chuoi ket thuc bang byte 0.
 */
void SH1106_DrawText(Sh1106 *d, uint8_t x, uint8_t y, const char *s);

/**
 * @brief Dinh vi va gui mot doan 32 byte tu framebuffer len OLED.
 * @param[in,out] d Doi tuong OLED da cau hinh.
 * @param[in] chunk Chi so doan tu 0 den SH1106_CHUNK_COUNT tru 1.
 * @retval 0 Gui thanh cong.
 * @return Ma loi khac 0 neu chi so sai hoac loi I2C.
 */
int SH1106_DisplayChunk(Sh1106 *d, uint8_t chunk);
#define SH1106_CHUNK_COUNT (SH1106_WIDTH * SH1106_HEIGHT / 8 / 32)

#ifdef __cplusplus
}
#endif

#endif
