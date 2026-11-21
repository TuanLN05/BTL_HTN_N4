#include "threshold_store.h"
#include "stm32f1xx.h"
#include <stddef.h>
#include <string.h>

// Hai page 1KB cuoi Flash 64KB; platformio.ini gioi han firmware 62KB.
#define PAGE0 0x0800F800UL
#define PAGE1 0x0800FC00UL
#define MAGIC 0x54485231UL
#define COMMIT 0xA55AU
#define ERRORS (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)
typedef struct {
    uint32_t magic, sequence;
    int32_t values[MENU_LIMIT_COUNT];
    uint32_t checksum;
    uint16_t commit, reserved;
} Record;
// Tinh ma kiem tra FNV-1a tren phan du lieu truoc truong checksum cua ban ghi.
static uint32_t checksum(const Record *r)
{
    const uint8_t *p = (const uint8_t *)r;
    uint32_t h = 2166136261UL;
    size_t i;
    for (i = 0; i < offsetof(Record, checksum); ++i) h = (h ^ p[i]) * 16777619UL;
    return h;
}
// Kiem tra tung nguong co nam trong khoang gia tri cho phep hay khong.
static int valid_values(const int32_t *v)
{
    return v[0] >= -400 && v[0] <= 850 && v[1] >= 0 && v[1] <= 1000 &&
           v[2] >= 3000 && v[2] <= 12000 && v[3] >= 0 && v[3] <= 4095;
}
// Kiem tra dau commit, magic, checksum va gia tri nguong cua ban ghi Flash.
static int valid(const Record *r)
{
    return r->commit == COMMIT && r->magic == MAGIC &&
           r->checksum == checksum(r) && valid_values(r->values);
}
static const Record *latest(void)
{
    const Record *a = (const Record *)PAGE0, *b = (const Record *)PAGE1;
    if (!valid(a)) return valid(b) ? b : NULL;
    if (!valid(b)) return a;
    return (int32_t)(b->sequence - a->sequence) > 0 ? b : a;
}
// Nap cac nguong tu ban ghi Flash moi nhat; tra -1 neu khong co du lieu hop le.
int ThresholdStore_Load(int32_t values[MENU_LIMIT_COUNT])
{
    const Record *r = latest();
    if (!r || !values) return -1;
    memcpy(values, r->values, sizeof(r->values));
    return 0;
}
// Cho Flash het ban va tra -1 neu co loi lap trinh hoac bao ve ghi.
static int wait_flash(void)
{
    while (FLASH->SR & FLASH_SR_BSY) {}
    return (FLASH->SR & ERRORS) ? -1 : 0;
}
// Luu nguong vao page Flash con lai, ghi commit cuoi cung de nhan dien ban ghi hoan tat.
int ThresholdStore_Save(const int32_t values[MENU_LIMIT_COUNT])
{
    const Record *old = latest();
    Record next;
    uint32_t target, primask;
    size_t offset;
    int rc = -1;
    if (!values || !valid_values(values)) return -1;
    // Khong ghi lai neu gia tri khong doi, giam hao mon Flash.
    if (old && memcmp(old->values, values, sizeof(old->values)) == 0) return 0;
    memset(&next, 0xFF, sizeof(next));
    next.magic = MAGIC;
    next.sequence = old ? old->sequence + 1 : 0;
    memcpy(next.values, values, sizeof(next.values));
    next.checksum = checksum(&next);
    next.commit = COMMIT;
    // Ghi page con lai de giu ban cu neu mat dien giua chung.
    target = old == (const Record *)PAGE0 ? PAGE1 : PAGE0;
    primask = __get_PRIMASK();
    __disable_irq();
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = 0x45670123UL;
        FLASH->KEYR = 0xCDEF89ABUL;
    }
    if (FLASH->CR & FLASH_CR_LOCK) goto done;
    (void)wait_flash();
    FLASH->SR = ERRORS | FLASH_SR_EOP;
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = target;
    FLASH->CR |= FLASH_CR_STRT;
    rc = wait_flash();
    FLASH->CR &= ~FLASH_CR_PER;
    if (rc != 0) goto done;
    FLASH->CR |= FLASH_CR_PG;
    // Commit ghi cuoi cung; ban ghi do dang se khong duoc nap.
    for (offset = 0; offset <= offsetof(Record, commit); offset += 2) {
        uint16_t half;
        memcpy(&half, (const uint8_t *)&next + offset, sizeof(half));
        *(volatile uint16_t *)(target + offset) = half;
        if (wait_flash() != 0 || *(volatile uint16_t *)(target + offset) != half) {
            rc = -1;
            break;
        }
    }
    FLASH->CR &= ~FLASH_CR_PG;
    if (rc == 0 && !valid((const Record *)target)) rc = -1;
done:
    FLASH->CR &= ~(FLASH_CR_PER | FLASH_CR_PG);
    FLASH->CR |= FLASH_CR_LOCK;
    __set_PRIMASK(primask);
    return rc;
}
