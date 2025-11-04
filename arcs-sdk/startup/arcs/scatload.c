#include <stdint.h>

extern uint32_t __scat_copy_base, __scat_copy_last, __scat_zero_base, __scat_zero_last;
extern uint32_t __psram_scat_copy_base, __psram_scat_copy_last, __psram_scat_zero_base, __psram_scat_zero_last;

typedef struct { uint32_t len, *vma, *lma; } scat_copy_item_t;
typedef struct { uint32_t len, *vma;       } scat_zero_item_t;

__attribute__((section(".init")))
static void _scatload(uint32_t *dst, const uint32_t *src, uint32_t cnt)
{
	while (cnt--) *dst++ = *src++;
}

__attribute__((section(".init")))
static void _scatfill(uint32_t *dst, uint32_t fill, int cnt)
{
	while (cnt--) *dst++ = fill;
}

__attribute__((section(".init")))
void scatload(void)
{
    for (scat_copy_item_t *item = (void *)&__scat_copy_base; (uint32_t *)item < &__scat_copy_last; item++)
        if (item->vma && item->len >= sizeof(uint32_t) && item->vma != item->lma)
            _scatload(item->vma, item->lma, item->len >> 2);

    
    for (scat_zero_item_t *item = (void *)&__scat_zero_base; (uint32_t *)item < &__scat_zero_last; item++)
        if (item->vma && item->len >= sizeof(uint32_t))
            _scatfill(item->vma, 0, item->len >> 2);
}

void scatload_psram(void)
{
    for (scat_copy_item_t *item = (void *)&__psram_scat_copy_base; (uint32_t *)item < &__psram_scat_copy_last; item++) {
        if (item->vma && item->len >= sizeof(uint32_t) && item->vma != item->lma) {
            _scatload(item->vma, item->lma, item->len >> 2);
        }
    }

    for (scat_zero_item_t *item = (void *)&__psram_scat_zero_base; (uint32_t *)item < &__psram_scat_zero_last; item++) {
        if (item->vma && item->len >= sizeof(uint32_t)) {
            _scatfill(item->vma, 0, item->len >> 2);
        }
    }
}
