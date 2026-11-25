#ifndef SH1106_H
#define SH1106_H

#include <stdint.h>

// Driver cho module SH1106 128x64, giao tiep I2C.
#define SH1106_WIDTH   128
#define SH1106_HEIGHT   64
#define SH1106_ADDR7  0x3C // dia chi I2C 7-bit

// Tham so data[0] da la byte dieu khien 0x00/0x40 cua SH1106.
// Return: 0 = I2C thanh cong; khac 0 = loi I2C.

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

// Cau hinh panel khi dang tat. Gui khung dau tien truoc khi bat panel.
int SH1106_Configure(Sh1106 *d, Sh1106I2CWrite write_fn, void *user, uint8_t address7);
int SH1106_PowerOn(Sh1106 *d);
void SH1106_Clear(Sh1106 *d);
void SH1106_DrawPixel(Sh1106 *d, uint8_t x, uint8_t y, uint8_t on);
void SH1106_DrawText(Sh1106 *d, uint8_t x, uint8_t y, const char *s);
// Moi doan 32 byte duoc dinh vi rieng de co the nha mutex giua cac doan.
int SH1106_DisplayChunk(Sh1106 *d, uint8_t chunk);
#define SH1106_CHUNK_COUNT (SH1106_WIDTH * SH1106_HEIGHT / 8 / 32)

#ifdef __cplusplus
}
#endif

#endif
