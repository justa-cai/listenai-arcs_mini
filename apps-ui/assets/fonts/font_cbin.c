#include "font_cbin.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "font_romfs.h"
#include "lisa_log.h"
#include "lv_port_mem.h"

#define CBIN_DSC_MIN_SIZE (20U)
#define CBIN_GLYPH_DSC_SIZE (16U)
#define CBIN_CMAP_ENTRY_SIZE (20U)

typedef struct {
    uint32_t dsc_off;
    int32_t line_height;
    int32_t base_line;
    uint8_t subpx;
    int8_t underline_position;
    int8_t underline_thickness;
} cbin_font_header_t;

static inline uint16_t read_u16(const uint8_t *ptr)
{
    uint16_t v = 0;

    memcpy(&v, ptr, sizeof(v));
    return v;
}

static inline int16_t read_s16(const uint8_t *ptr)
{
    int16_t v = 0;

    memcpy(&v, ptr, sizeof(v));
    return v;
}

static inline uint32_t read_u32(const uint8_t *ptr)
{
    uint32_t v = 0;

    memcpy(&v, ptr, sizeof(v));
    return v;
}

static inline bool range_ok(uint32_t off, uint32_t len, uint32_t total)
{
    return (off <= total) && (len <= (total - off));
}

static inline void *lv_zalloc(size_t sz)
{
    void *p = lv_mem_alloc(sz);

    if (p) {
        memset(p, 0, sz);
    }
    return p;
}

static inline void *memdup_lv(const void *src, size_t sz)
{
    void *dst = lv_mem_alloc(sz);

    if (!dst) {
        return NULL;
    }

    memcpy(dst, src, sz);
    return dst;
}

static inline void addr_add(void **addr, uintptr_t add)
{
    if (*addr) {
        *addr = (void *)((uintptr_t)(*addr) + add);
    }
}

static bool parse_cbin_header(const uint8_t *bin, uint32_t size, cbin_font_header_t *out)
{
    const uint32_t dsc_off_v8 = read_u32(bin + 20U);
    const uint32_t dsc_off_v9 = read_u32(bin + 24U);

    if (dsc_off_v9 > 0U && dsc_off_v9 < 0x100U && range_ok(dsc_off_v9, CBIN_DSC_MIN_SIZE, size)) {
        out->dsc_off = dsc_off_v9;
        out->line_height = (int32_t)read_u32(bin + 12U);
        out->base_line = (int32_t)read_u32(bin + 16U);
        out->subpx = *(bin + 20U);
        out->underline_position = (int8_t)*(bin + 21U);
        out->underline_thickness = (int8_t)*(bin + 22U);
        return true;
    }

    if (dsc_off_v8 > 0U && dsc_off_v8 < 0x100U && range_ok(dsc_off_v8, CBIN_DSC_MIN_SIZE, size)) {
        out->dsc_off = dsc_off_v8;
        out->line_height = (int32_t)read_u32(bin + 8U);
        out->base_line = (int32_t)read_u32(bin + 12U);
        out->subpx = *(bin + 16U);
        out->underline_position = (int8_t)*(bin + 17U);
        out->underline_thickness = (int8_t)*(bin + 18U);
        return true;
    }

    return false;
}

static void cbin_font_delete(lv_font_t *font)
{
    lv_font_fmt_txt_dsc_t *dsc;

    if (!font) {
        return;
    }

    dsc = (lv_font_fmt_txt_dsc_t *)font->dsc;
    if (dsc) {
        lv_mem_free((void *)dsc->cmaps);
        lv_mem_free((void *)dsc->kern_dsc);
        lv_mem_free((void *)dsc->glyph_dsc);
        lv_mem_free((void *)dsc);
    }
    lv_mem_free(font);
}

static lv_font_t *cbin_font_create(const uint8_t *bin, uint32_t size)
{
    cbin_font_header_t hdr;
    lv_font_t *font;
    lv_font_fmt_txt_dsc_t *dsc;
    const uint8_t *dsc_addr;
    uint32_t glyph_bitmap_off;
    uint32_t glyph_dsc_off;
    uint32_t cmaps_off;
    uint32_t kern_off;
    uint16_t kern_scale;
    uint16_t packed_info;
    uint16_t cmap_num;
    uint8_t bpp;
    uint8_t kern_classes;
    uint8_t bitmap_format;

    if (!bin || size < 64U) {
        return NULL;
    }

    if (!parse_cbin_header(bin, size, &hdr)) {
        LOGE("unsupported cbin header");
        return NULL;
    }

    dsc_addr = bin + hdr.dsc_off;
    glyph_bitmap_off = read_u32(dsc_addr + 0U);
    glyph_dsc_off = read_u32(dsc_addr + 4U);
    cmaps_off = read_u32(dsc_addr + 8U);
    kern_off = read_u32(dsc_addr + 12U);
    kern_scale = read_u16(dsc_addr + 16U);
    packed_info = read_u16(dsc_addr + 18U);

    cmap_num = (uint16_t)(packed_info & 0x01FFU);
    bpp = (uint8_t)((packed_info >> 9U) & 0x0FU);
    kern_classes = (uint8_t)((packed_info >> 13U) & 0x01U);
    bitmap_format = (uint8_t)((packed_info >> 14U) & 0x03U);

    if (!range_ok(hdr.dsc_off + glyph_dsc_off, CBIN_GLYPH_DSC_SIZE, size) ||
        !range_ok(hdr.dsc_off + cmaps_off, (uint32_t)cmap_num * CBIN_CMAP_ENTRY_SIZE, size) ||
        cmaps_off <= glyph_dsc_off) {
        LOGE("invalid cbin font offsets");
        return NULL;
    }

    font = (lv_font_t *)lv_zalloc(sizeof(lv_font_t));
    dsc = (lv_font_fmt_txt_dsc_t *)lv_zalloc(sizeof(lv_font_fmt_txt_dsc_t));
    if (!font || !dsc) {
        lv_mem_free(font);
        lv_mem_free(dsc);
        return NULL;
    }

    font->get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt;
    font->get_glyph_bitmap = lv_font_get_bitmap_fmt_txt;
    font->line_height = (lv_coord_t)hdr.line_height;
    font->base_line = (lv_coord_t)hdr.base_line;
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    font->subpx = hdr.subpx;
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    font->underline_position = hdr.underline_position;
    font->underline_thickness = hdr.underline_thickness;
#endif
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    font->fallback = NULL;
#endif
#if LV_USE_USER_DATA
    font->user_data = NULL;
#endif
    font->dsc = dsc;

    dsc->glyph_bitmap = bin + hdr.dsc_off + glyph_bitmap_off;
    dsc->kern_scale = kern_scale;
    dsc->cmap_num = cmap_num;
    dsc->bpp = bpp;
    dsc->kern_classes = kern_classes;
    dsc->bitmap_format = bitmap_format;

    {
        const uint8_t *src_glyph = bin + hdr.dsc_off + glyph_dsc_off;
        uint32_t glyph_cnt = (cmaps_off - glyph_dsc_off) / CBIN_GLYPH_DSC_SIZE;
        lv_font_fmt_txt_glyph_dsc_t *glyph_dsc =
            (lv_font_fmt_txt_glyph_dsc_t *)lv_zalloc(sizeof(lv_font_fmt_txt_glyph_dsc_t) * glyph_cnt);

        if (!glyph_dsc) {
            cbin_font_delete(font);
            return NULL;
        }

        for (uint32_t i = 0; i < glyph_cnt; i++) {
            const uint8_t *ent = src_glyph + i * CBIN_GLYPH_DSC_SIZE;

            glyph_dsc[i].bitmap_index = read_u32(ent + 0U);
            glyph_dsc[i].adv_w = read_u32(ent + 4U);
            glyph_dsc[i].box_w = read_u16(ent + 8U);
            glyph_dsc[i].box_h = read_u16(ent + 10U);
            glyph_dsc[i].ofs_x = read_s16(ent + 12U);
            glyph_dsc[i].ofs_y = read_s16(ent + 14U);
        }

        dsc->glyph_dsc = glyph_dsc;
    }

    if (cmap_num > 0U) {
        const uint8_t *cmaps_addr = bin + hdr.dsc_off + cmaps_off;
        lv_font_fmt_txt_cmap_t *cmaps =
            (lv_font_fmt_txt_cmap_t *)lv_zalloc(sizeof(lv_font_fmt_txt_cmap_t) * cmap_num);

        if (!cmaps) {
            cbin_font_delete(font);
            return NULL;
        }

        for (uint16_t i = 0; i < cmap_num; i++) {
            const uint8_t *ptr = cmaps_addr + (uint32_t)i * CBIN_CMAP_ENTRY_SIZE;
            uint32_t unicode_off;
            uint32_t glyph_id_ofs_off;

            cmaps[i].range_start = read_u32(ptr + 0U);
            cmaps[i].range_length = read_u16(ptr + 4U);
            cmaps[i].glyph_id_start = read_u16(ptr + 6U);
            unicode_off = read_u32(ptr + 8U);
            glyph_id_ofs_off = read_u32(ptr + 12U);
            cmaps[i].list_length = read_u16(ptr + 16U);
            cmaps[i].type = (lv_font_fmt_txt_cmap_type_t)ptr[18];

            cmaps[i].unicode_list = unicode_off ? (const uint16_t *)(cmaps_addr + unicode_off) : NULL;
            cmaps[i].glyph_id_ofs_list = glyph_id_ofs_off ? (const void *)(cmaps_addr + glyph_id_ofs_off) : NULL;
        }

        dsc->cmaps = cmaps;
    }

    if (kern_off != 0U) {
        const uint8_t *kern_addr = bin + hdr.dsc_off + kern_off;

        if (kern_classes == 1U) {
            lv_font_fmt_txt_kern_classes_t *kcl =
                (lv_font_fmt_txt_kern_classes_t *)memdup_lv(kern_addr, sizeof(lv_font_fmt_txt_kern_classes_t));

            if (!kcl) {
                cbin_font_delete(font);
                return NULL;
            }

            dsc->kern_dsc = kcl;
            addr_add((void **)&kcl->class_pair_values, (uintptr_t)kern_addr);
            addr_add((void **)&kcl->left_class_mapping, (uintptr_t)kern_addr);
            addr_add((void **)&kcl->right_class_mapping, (uintptr_t)kern_addr);
        } else {
            lv_font_fmt_txt_kern_pair_t *kp =
                (lv_font_fmt_txt_kern_pair_t *)memdup_lv(kern_addr, sizeof(lv_font_fmt_txt_kern_pair_t));

            if (!kp) {
                cbin_font_delete(font);
                return NULL;
            }

            dsc->kern_dsc = kp;
            addr_add((void **)&kp->glyph_ids, (uintptr_t)kern_addr);
            addr_add((void **)&kp->values, (uintptr_t)kern_addr);
        }
    }

    return font;
}

lv_font_t *font_cbin_load(const char *path)
{
    uint8_t *font_data = NULL;
    uint32_t font_size = 0;

    if (!font_romfs_get(path, &font_data, &font_size)) {
        LOGW("cbin font not found: %s", path ? path : "(null)");
        return NULL;
    }

    return cbin_font_create(font_data, font_size);
}

lv_font_t *font_cbin_get(font_cbin_cache_t *cache)
{
    if (!cache) {
        return NULL;
    }

    if (cache->font) {
        return cache->font;
    }

    if (cache->load_attempted) {
        return NULL;
    }
    cache->load_attempted = true;

    cache->font = font_cbin_load(cache->path);
    if (!cache->font) {
        LOGE("font_cbin_load failed");
    }

    return cache->font;
}
