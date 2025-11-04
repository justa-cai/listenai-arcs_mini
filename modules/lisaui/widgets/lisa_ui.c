#include "lisa_ui.h"
#include "lisa_ui_assets.h"

#define LV_THEME_DEFAULT_COLOR_MAIN lv_color_hex(0x4F2814)
#define LV_THEME_DEFAULT_COLOR_SECONDARY lv_color_hex(0x4F2814)

void lisa_ui_init(void)
{
    // lv_theme_t * th = lv_theme_default_init(lv_disp_get_default(),
    //     LV_THEME_DEFAULT_COLOR_MAIN, LV_THEME_DEFAULT_COLOR_SECONDARY,
    //     false,
    //     &lv_font_chinese_18);

    // lv_disp_set_theme(lv_disp_get_default(), th);

    lisa_ui_assets_init();
}
