#include <stdio.h>

#include "arcs_ap_base.h"
#include "sysheap.h"
#include "lsfs.h"
#include "lvgl.h"
#include "lv_port_mem.h"
#include "lisa_log.h"

#include "romfs/romfs.h"

typedef struct{
    uint16_t min;
    uint16_t max;
    uint8_t  bpp;
    uint8_t  reserved[3];
}x_header_t;
typedef struct{
    uint8_t adv_w;
    uint8_t box_w;
    uint8_t box_h;
    int8_t  ofs_x;
    int8_t  ofs_y;
    uint8_t r;
}glyph_dsc_t;

static x_header_t __g_xbf_hd = {
    .min = 0x0020,
    .max = 0xffe5,
    .bpp = 4,
};

#define RESPAK_BIN_ADDR     (CMN_FLASH_REGION + 0x00400000)
#define RESPAK_BIN_SIZE     (2 * 1024 * 1024)

static uint32_t s_font_romfs_addr = RESPAK_BIN_ADDR;
static uint32_t s_font_romfs_size = RESPAK_BIN_SIZE;
static struct romfs *s_font_romfs = NULL;
static uint8_t *s_pFont = NULL;

static uint8_t *__user_font_getdata(int offset, int size)
{
    if (!s_pFont)
    {

        if (s_font_romfs_addr == 0 || s_font_romfs_size == 0) {
            return NULL;
        }

        if (romfs_init(&s_font_romfs, (const void *)(uintptr_t)s_font_romfs_addr, s_font_romfs_size) != 0) {
            return NULL;
        }
        if (!s_font_romfs) {
            return NULL;
        }
        uint8_t *pFont = NULL;
        uint32_t fontSize = 0;
        (void)romfs_info_get(s_font_romfs, "/font/lv_font_chinese_18.bin", (uint8_t **)&pFont, &fontSize);
        LOGI("romfs_info_get lv_font_chinese_18.bin: pFont=%p, fontSize=%u", pFont, fontSize);
        LOGI("romfs region: base=0x%08x, size=0x%08x", (unsigned int)s_font_romfs_addr, (unsigned int)s_font_romfs_size);

        if (pFont && fontSize > 0) {
            s_pFont = pFont;
            LOGI("lv_font_chinese_18.bin mapped addr=%p", s_pFont);
        }
        romfs_deinit(&s_font_romfs);

        if (!s_pFont) {
            return NULL;
        }
    }
    
    return s_pFont + offset;
}

static const uint8_t * __user_font_get_bitmap(const lv_font_t * font, uint32_t unicode_letter) {
    if( unicode_letter>__g_xbf_hd.max || unicode_letter<__g_xbf_hd.min ) {
        return NULL;
    }
    uint32_t unicode_offset = sizeof(x_header_t)+(unicode_letter-__g_xbf_hd.min)*4;
    uint32_t *p_pos = (uint32_t *)__user_font_getdata(unicode_offset, 4);
    if( p_pos[0] != 0 ) {
        uint32_t pos = p_pos[0];
        glyph_dsc_t * gdsc = (glyph_dsc_t*)__user_font_getdata(pos, sizeof(glyph_dsc_t));
        return __user_font_getdata(pos+sizeof(glyph_dsc_t), gdsc->box_w*gdsc->box_h*__g_xbf_hd.bpp/8);
    }
    return NULL;
}


static bool __user_font_get_glyph_dsc(const lv_font_t * font, lv_font_glyph_dsc_t * dsc_out, uint32_t unicode_letter, uint32_t unicode_letter_next) {
    if( unicode_letter>__g_xbf_hd.max || unicode_letter<__g_xbf_hd.min ) {
        return NULL;
    }
    uint32_t unicode_offset = sizeof(x_header_t)+(unicode_letter-__g_xbf_hd.min)*4;
    uint32_t *p_pos = (uint32_t *)__user_font_getdata(unicode_offset, 4);
    if( p_pos[0] != 0 ) {
        glyph_dsc_t * gdsc = (glyph_dsc_t*)__user_font_getdata(p_pos[0], sizeof(glyph_dsc_t));
        dsc_out->adv_w = gdsc->adv_w;
        dsc_out->box_h = gdsc->box_h;
        dsc_out->box_w = gdsc->box_w;
        dsc_out->ofs_x = gdsc->ofs_x;
        dsc_out->ofs_y = gdsc->ofs_y;
        dsc_out->bpp   = __g_xbf_hd.bpp;
        return true;
    }
    return false;
}

//字体名称: ChillRoundGothic_Medium.otf
//字模高度: 18
//外部 bin 文件,字体信息等级: Level0
lv_font_t lv_font_chinese_18 = {
    .get_glyph_bitmap = __user_font_get_bitmap,
    .get_glyph_dsc = __user_font_get_glyph_dsc,
    .line_height = 18,
    .base_line = 2,
};