#include "sh1106.h"
#include <stddef.h>
#include <string.h>

// Font 5x7. Moi ky tu rong 6 pixel, cao 8 pixel.
// 128 pixel / 6 = 21 ky tu / dong; 64 pixel / 8 = 8 dong.
static const uint8_t digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}
};
static const uint8_t letters[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
};
static const uint8_t punct_blank[5]={0,0,0,0,0};
static const uint8_t punct_colon[5]={0,0x36,0x36,0,0};
static const uint8_t punct_dot[5]={0,0x60,0x60,0,0};
static const uint8_t punct_dash[5]={0x08,0x08,0x08,0x08,0x08};
static const uint8_t punct_slash[5]={0x20,0x10,0x08,0x04,0x02};
static const uint8_t punct_percent[5]={0x63,0x13,0x08,0x64,0x63};
static const uint8_t punct_gt[5]={0x00,0x41,0x22,0x14,0x08};
static const uint8_t punct_lt[5]={0x08,0x14,0x22,0x41,0x00};
static const uint8_t punct_bang[5]={0,0,0x5F,0,0};
static const uint8_t punct_equal[5]={0x14,0x14,0x14,0x14,0x14};
static const uint8_t punct_plus[5]={0x08,0x08,0x3E,0x08,0x08};
static const uint8_t punct_question[5]={0x02,0x01,0x51,0x09,0x06};
static const uint8_t punct_lparen[5]={0,0x1C,0x22,0x41,0};
static const uint8_t punct_rparen[5]={0,0x41,0x22,0x1C,0};
static const uint8_t punct_under[5]={0x40,0x40,0x40,0x40,0x40};
static const uint8_t punct_comma[5]={0,0x50,0x30,0,0};
static const uint8_t punct_star[5]={0x14,0x08,0x3E,0x08,0x14};

static const uint8_t *glyph(char ch) {
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch-'a'+'A');
    if (ch >= 'A' && ch <= 'Z') return letters[ch-'A'];
    if (ch >= '0' && ch <= '9') return digits[ch-'0'];
    switch(ch) {
        case ':': return punct_colon;
        case '.': return punct_dot;
        case '-': return punct_dash;
        case '/': return punct_slash;
        case '%': return punct_percent;
        case '>': return punct_gt;
        case '<': return punct_lt;
        case '!': return punct_bang;
        case '=': return punct_equal;
        case '+': return punct_plus;
        case '?': return punct_question;
        case '(': return punct_lparen;
        case ')': return punct_rparen;
        case '_': return punct_under;
        case ',': return punct_comma;
        case '*': return punct_star;
        default: return punct_blank;
    }
}


// 0x00 = sau do la LENH; 0x40 = sau do la DU LIEU HIEN THI.
// Callback d->write thuc hien giao dich I2C.
static int send_commands(Sh1106 *d, const uint8_t *cmd, uint8_t count)
{
    uint8_t tx[26];
    if (!d || !d->write || count == 0 || count > 25) return -1;
    tx[0] = 0x00;
    memcpy(&tx[1], cmd, count);
    d->last_error = d->write(d->address7, tx, (uint16_t)(count + 1), d->user);
    return d->last_error;
}

// Gui du lieu hien thi qua callback I2C voi byte dieu khien 0x40.
static int send_data(Sh1106 *d, const uint8_t *src, uint8_t count)
{
    uint8_t tx[33];
    if (!d || !d->write || count == 0 || count > 32) return -1;
    tx[0] = 0x40;
    memcpy(&tx[1], src, count);
    d->last_error = d->write(d->address7, tx, (uint16_t)(count + 1), d->user);
    return d->last_error;
}

int SH1106_Configure(Sh1106 *d, Sh1106I2CWrite write_fn, void *user, uint8_t address7)
{
    // SH1106 128x64: bat DC-DC, offset hang 0, cau hinh COM cho 64 hang.
    static const uint8_t init[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F,
        0xD3, 0x00, 0x40, 0xAD, 0x8B,
        0xA1, 0xC8, 0xDA, 0x12, 0x81, 0x80,
        0xD9, 0x22, 0xDB, 0x35, 0xA4, 0xA6
    };
    if (!d || !write_fn || address7 > 0x7F) return -1;
    memset(d, 0, sizeof(*d));
    d->write = write_fn;
    d->user = user;
    d->address7 = address7;
    return send_commands(d, init, (uint8_t)sizeof(init));
}

int SH1106_PowerOn(Sh1106 *d)
{
    const uint8_t on = 0xAF;
    return send_commands(d, &on, 1);
}

// Xoa bo dem anh trong RAM; can goi SH1106_Display de cap nhat OLED.
void SH1106_Clear(Sh1106 *d)
{
    // Chi xoa bo dem trong RAM.
    if (d) memset(d->framebuffer, 0, sizeof(d->framebuffer));
}

// Bat hoac tat mot diem anh trong bo dem RAM, bo qua toa do ngoai man hinh.
void SH1106_DrawPixel(Sh1106 *d, uint8_t x, uint8_t y, uint8_t on)
{
    uint16_t index;
    uint8_t mask;
    if (!d || x >= SH1106_WIDTH || y >= SH1106_HEIGHT) return;

    // Moi page gom 8 hang pixel, 128 cot = 128 byte.
    // Byte index = (y/8)*128 + x; bit y%8 la 1 diem anh.
    index = (uint16_t)(y / 8) * SH1106_WIDTH + x;
    mask = (uint8_t)(1 << (y & 7));
    if (on) d->framebuffer[index] |= mask;
    else    d->framebuffer[index] &= (uint8_t)~mask;
}

// Ve chuoi ky tu font 5x7 vao bo dem anh tai toa do cho truoc.
void SH1106_DrawText(Sh1106 *d, uint8_t x, uint8_t y, const char *s)
{
    // Ve tung diem cua font vao framebuffer, chua gui I2C o buoc nay.
    uint8_t col, bit;
    if (!d || !s || y >= SH1106_HEIGHT) return;
    while (*s && x <= SH1106_WIDTH - 6) {
        const uint8_t *p = glyph(*s++);
        for (col = 0; col < 5; ++col) {
            for (bit = 0; bit < 7; ++bit) {
                if ((p[col] >> bit) & 1)
                    SH1106_DrawPixel(d, (uint8_t)(x + col),
                                     (uint8_t)(y + bit), 1);
            }
        }
        x = (uint8_t)(x + 6);
    }
}

int SH1106_DisplayChunk(Sh1106 *d, uint8_t chunk)
{
    if (!d || !d->write || chunk >= SH1106_CHUNK_COUNT) return -1;
    const uint16_t offset = (uint16_t)chunk * 32U;
    const uint8_t page = offset / SH1106_WIDTH;
    const uint8_t column = offset % SH1106_WIDTH + 2U;
    const uint8_t pos[] = {
        (uint8_t)(0xB0 | page), (uint8_t)(0x10 | (column >> 4)), (uint8_t)(column & 0x0F)
    };
    if (send_commands(d, pos, sizeof(pos)) != 0) return -1;
    return send_data(d, &d->framebuffer[offset], 32);
}
