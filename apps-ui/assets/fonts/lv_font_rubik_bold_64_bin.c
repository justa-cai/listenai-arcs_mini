#include "font_cbin.h"

#define FONT_CBIN_PATH "/font/lv_font_rubik_bold_64.bin"

static font_cbin_cache_t s_font_cache = FONT_CBIN_CACHE_INIT(FONT_CBIN_PATH);

static const uint8_t *__user_font_get_bitmap(const lv_font_t *font, uint32_t unicode_letter)
{
    lv_font_t *runtime_font;

    (void)font;

    runtime_font = font_cbin_get(&s_font_cache);
    if (!runtime_font) {
        return NULL;
    }

    return runtime_font->get_glyph_bitmap(runtime_font, unicode_letter);
}

static bool __user_font_get_glyph_dsc(const lv_font_t *font,
                                      lv_font_glyph_dsc_t *dsc_out,
                                      uint32_t unicode_letter,
                                      uint32_t unicode_letter_next)
{
    lv_font_t *runtime_font;

    (void)font;

    runtime_font = font_cbin_get(&s_font_cache);
    if (!runtime_font) {
        return false;
    }

    return runtime_font->get_glyph_dsc(runtime_font, dsc_out, unicode_letter, unicode_letter_next);
}

// 字体名称: ChillRoundGothic_Medium.otf
// 字模高度: 16
lv_font_t lv_font_rubik_bold_64 = {
    .get_glyph_bitmap = __user_font_get_bitmap,
    .get_glyph_dsc = __user_font_get_glyph_dsc,
    .line_height = 64,
    .base_line = 0,
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = NULL,
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
#if LV_USE_USER_DATA
    .user_data = NULL,
#endif
};
