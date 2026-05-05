#include <stddef.h>
#include <stdint.h>

extern uint32_t __boot_f_scat_copy_base, __boot_f_scat_copy_last;
extern uint32_t __boot_f_scat_zero_base, __boot_f_scat_zero_last;
extern uint32_t __scat_copy_base, __scat_copy_last;
extern uint32_t __scat_zero_base, __scat_zero_last;
extern uint32_t __psram_scat_copy_base, __psram_scat_copy_last;
extern uint32_t __psram_scat_zero_base, __psram_scat_zero_last;

typedef struct { uint32_t len, *vma, *lma; } scat_copy_item_t;
typedef struct { uint32_t len, *vma; } scat_zero_item_t;

#define SCATLOAD_SECTION_INIT        ".init"
#define SCATLOAD_SECTION_BOOT_RAM    ".boot_f.ramcode"

#define DEFINE_SCATLOAD_COPY(name, section_name)                                                                  \
    __attribute__((optimize("O0"), section(section_name))) static void name(uint32_t *dst,                       \
                                                                             const uint32_t *src, uint32_t cnt)   \
    {                                                                                                             \
        while (cnt--)                                                                                             \
            *dst++ = *src++;                                                                                      \
    }

#define DEFINE_SCATLOAD_FILL(name, section_name)                                                                  \
    __attribute__((optimize("O0"), section(section_name))) static void name(uint32_t *dst, uint32_t fill,        \
                                                                             uint32_t cnt)                        \
    {                                                                                                             \
        while (cnt--)                                                                                             \
            *dst++ = fill;                                                                                        \
    }

DEFINE_SCATLOAD_COPY(_scatload_boot_f, SCATLOAD_SECTION_INIT)
DEFINE_SCATLOAD_FILL(_scatfill_boot_f, SCATLOAD_SECTION_INIT)
DEFINE_SCATLOAD_COPY(_scatload_runtime, SCATLOAD_SECTION_BOOT_RAM)
DEFINE_SCATLOAD_FILL(_scatfill_runtime, SCATLOAD_SECTION_BOOT_RAM)

#define SCATLOAD_APPLY_COPY(copy_base, copy_last, copy_fn)                                                       \
    do {                                                                                                          \
        const scat_copy_item_t *item;                                                                             \
                                                                                                                  \
        for (item = (const scat_copy_item_t *)(copy_base); item < (const scat_copy_item_t *)(copy_last); item++) { \
            if (item->vma != NULL && item->len >= sizeof(uint32_t) && item->vma != item->lma) {                 \
                copy_fn(item->vma, item->lma, item->len >> 2);                                                    \
            }                                                                                                     \
        }                                                                                                         \
    } while (0)

#define SCATLOAD_APPLY_ZERO(zero_base, zero_last, fill_fn)                                                       \
    do {                                                                                                          \
        const scat_zero_item_t *item;                                                                             \
                                                                                                                  \
        for (item = (const scat_zero_item_t *)(zero_base); item < (const scat_zero_item_t *)(zero_last); item++) { \
            if (item->vma != NULL && item->len >= sizeof(uint32_t)) {                                            \
                fill_fn(item->vma, 0, item->len >> 2);                                                            \
            }                                                                                                     \
        }                                                                                                         \
    } while (0)

__attribute__((section(".boot_f.ramcode"))) void scatload(void)
{
    SCATLOAD_APPLY_COPY(&__scat_copy_base, &__scat_copy_last, _scatload_runtime);
    SCATLOAD_APPLY_ZERO(&__scat_zero_base, &__scat_zero_last, _scatfill_runtime);
}

__attribute__((section(".init"))) void scatload_boot_f(void)
{
    SCATLOAD_APPLY_COPY(&__boot_f_scat_copy_base, &__boot_f_scat_copy_last, _scatload_boot_f);
    SCATLOAD_APPLY_ZERO(&__boot_f_scat_zero_base, &__boot_f_scat_zero_last, _scatfill_boot_f);
}

__attribute__((section(".boot_f.ramcode"))) void scatload_psram(void)
{
    SCATLOAD_APPLY_COPY(&__psram_scat_copy_base, &__psram_scat_copy_last, _scatload_runtime);
    SCATLOAD_APPLY_ZERO(&__psram_scat_zero_base, &__psram_scat_zero_last, _scatfill_runtime);
}
