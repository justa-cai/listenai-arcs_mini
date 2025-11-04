#include "lvgl.h"
#include "lisa_ui.h"

#define DEFAULT_THEME_COLOR           lv_color_hex(0x000000)
#define DEFAULT_THEME_COLOR_SECONDARY lv_color_hex(0x000000)
#define DEFAULT_THEME_FONT            &lv_font_chinese_18

static lv_style_t g_style_bg_color;
static lv_style_t style_btn;
static lv_style_t style_btn_pressed;

void lisa_ui_default_theme_init(void)
{
    lv_style_init(&g_style_bg_color);
    lv_style_set_bg_color(&g_style_bg_color, DEFAULT_THEME_COLOR);
    lv_style_set_bg_opa(&g_style_bg_color, LV_OPA_COVER);

    /* 按钮默认状态 */
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, lv_color_hex(0xfd8926));
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_radius(&style_btn, 10);
    lv_style_set_border_width(&style_btn, 1);
    lv_style_set_text_color(&style_btn, lv_color_white());

    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_color(&style_btn_pressed, lv_color_hex(0xfd8926));
    lv_style_set_bg_opa(&style_btn_pressed, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_pressed, 10);
    lv_style_set_border_width(&style_btn_pressed, 1);
    lv_style_set_text_color(&style_btn_pressed, lv_color_white());
}



lv_theme_t *lisa_ui_default_theme_get(lv_disp_t *disp)
{
    lv_theme_t *th =
        lv_theme_default_init(disp, DEFAULT_THEME_COLOR, DEFAULT_THEME_COLOR_SECONDARY, false, DEFAULT_THEME_FONT);

    lv_style_init(&g_style_bg_color);

    return th;
}
