#include "lvgl.h"
#include <stdio.h>
// #include "lv_port_file.h"
#include "lsfs.h"
#include "log_print.h"
#include "lv_port_mem.h"
#include "sysheap.h"

#include "lisaui_asset_manager.h"

typedef struct{
    uint16_t min;
    uint16_t max;
    uint8_t  bpp;
    uint8_t  reserved[3];
}x_header_t;
typedef struct{
    uint32_t pos;
}x_table_t;
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

char *Font_buff = NULL;
#if 0
static void * init_font(void)
{
	FILE *ff = fopen("/SD:/font/lv_font_chinese_18.bin", "r");
	if (ff == NULL)
	{
		CLOGE("Failed to open file for font res");
		return NULL;
	}
	fseek(ff, 0, SEEK_END);
	long lSize = ftell(ff);
	rewind(ff);
	CLOGD("lsfs size %ld", lSize);
	static uint8_t first_in = 1;
	if (first_in == 1)
	{
		first_in = 0;
		Font_buff = (char *)exram_malloc(32, (sizeof(char) * lSize));
	}
	int br = fread(Font_buff, 1, lSize, ff);
	CLOGD("Bytes read %d", br);
	fclose(ff);
	return Font_buff;
}
#else

void *init_font(void)
{
    struct lsfs_file_t file;
    int ret;

    lsfs_file_t_init(&file);
    ret = lsfs_open(&file, "/SD:/font/lv_font_chinese_18.bin", LSFS_O_READ);
    if (ret != 0) {
        CLOGE("Failed to open file: %d", ret);
        return NULL;
    }

    lsfs_seek(&file, 0, SEEK_END);
    long lSize = lsfs_tell(&file);
    CLOGI("Lsize %ld", lSize);

    static uint8_t first_in = 1;
    if (first_in == 1) {
        first_in = 0;
        Font_buff = (char *)exram_malloc(32, (sizeof(char) * lSize));
    }
    if (Font_buff == NULL) {
        CLOGE("[%s] malloc falid", __FUNCTION__);
        return NULL;
    }

    lsfs_seek(&file, 0, SEEK_SET);
    int br = lsfs_read(&file, Font_buff, lSize);
    if (br != lSize) {
        CLOGE("[%s] lsfs_read falid", __FUNCTION__);
        return NULL;
    }

    lsfs_close(&file);
    return Font_buff;
}

#endif

void flash_copy_font_res(void)
{
    CLOGD("flash_copy_font_res");
    Font_buff = (char *)exram_malloc(32, 1858949);
    if (!Font_buff) {
        CLOGE("exram_malloc Failed");
        return;
    }
    memcpy(Font_buff, (void *)0x30C00000, 1858949);
    CLOGD("flash_copy_font_res Font_buff:%p", Font_buff);
}

static lisaui_resource_item_t g_font_res_item;

static uint8_t *__user_font_getdata(int offset, int size)
{
    // uint32_t font_address = CONFIG_FLASH_BASE_ADDRESS + CONFIG_FLASH_RES_LV_FONT_CHINESE_ADDRESS + offset;

    if (!Font_buff) {
        // init_font();
        if (g_font_res_item.is_loaded == false) {
            CLOGD("Font_buff is NULL, load font res from emmc");
            g_font_res_item.type = LISAUI_RES_TYPE_BIN_FONT;
            g_font_res_item.load_dst = LISAUI_RES_LOAD_DST_PSRAM; // 默认加载到 PSRAM
            lisaui_asset_manager_get_asset_by_emmc_path("/SD:/font/lv_font_chinese_18.bin", &g_font_res_item);
            if (g_font_res_item.is_loaded == false) {
                CLOGE("Failed to load font resource from emmc");
                return NULL;
            } else {
                Font_buff = g_font_res_item.data;
            }
        }
        // flash_copy_font_res();
    }
    return (uint8_t *)Font_buff + offset;

    // uint8_t *pFont = (uint8_t *)0x30C00000;
    // return pFont + offset;
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
