#include "ls_wifi_list.h"
#include <src/widgets/lv_label.h>
#include "lv_img_utils.h"
#include "incbin.h"

#if CONFIG_LVGL_ENV_SIMULATOR
#define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "app/llm/lisaui/app/app_wifi/" path
#else
// #define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "src/ui/lisaui/app/app_wifi/" path
#define LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH(path) "source/view/lisaui/app/app_wifi/" path
#endif

UI_RES_IMG_NAME(wifi_status, LISAUI_APP_LS_LLM_DIALOG_UI_RES_PERFIX_PATH("assets/png/ic_status_wifi4.png"))

lv_obj_t *ls_ui_wifi_item_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lisa_ui_wifi_item_t *wifi_item = lv_mem_realloc(obj, sizeof(lisa_ui_wifi_item_t));

    lv_obj_t *item = (lv_obj_t *)wifi_item;

    if (wifi_item == NULL) {
        lv_obj_del(obj);
        return NULL;
    }

    lv_obj_set_size(item, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(item, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x24242f), LV_PART_MAIN | LV_STATE_DEFAULT);

    wifi_item->ssid_lbl = lv_label_create(item);
    lv_obj_set_size(wifi_item->ssid_lbl, LV_PCT(60), LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(wifi_item->ssid_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(wifi_item->ssid_lbl, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(wifi_item->ssid_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);

    lv_obj_t *op_btn = lv_btn_create(item);
    lv_obj_set_size(op_btn, LV_PCT(20), LV_SIZE_CONTENT);
    lv_obj_align(op_btn, LV_ALIGN_RIGHT_MID, LV_DPX(0), 0);
    lv_obj_set_style_bg_color(op_btn, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(op_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);


    wifi_item->img = lv_img_create(item);
    lv_img_png_src_init(UI_RES_IMG_PNG(wifi_status));
    lv_img_set_src(wifi_item->img, &LV_IMG_DSC(wifi_status));
    lv_obj_add_flag(wifi_item->img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(wifi_item->img, LV_ALIGN_RIGHT_MID, LV_DPX(0), 0);
    lv_obj_set_style_radius(wifi_item->img, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(wifi_item->img, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);

    wifi_item->sta_lbl = lv_label_create(item);
    lv_obj_align_to(wifi_item->sta_lbl, wifi_item->img, LV_ALIGN_OUT_LEFT_MID, -LV_DPX(40), 0);
    lv_obj_set_style_text_font(wifi_item->sta_lbl, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(wifi_item->sta_lbl, lv_color_hex(0x2BBDA0), LV_PART_MAIN | LV_STATE_DEFAULT);

    return (lv_obj_t *)wifi_item;
}

void ls_ui_wifi_item_set_ssid(lisa_ui_wifi_item_t *wifi_item, const char *ssid)
{
    if (wifi_item == NULL) {
        return;
    }

    lv_label_set_text(wifi_item->ssid_lbl, ssid);
}

void ls_ui_wifi_item_set_status(lisa_ui_wifi_item_t *wifi_item, const char *status)
{
    if (wifi_item == NULL) {
        return;
    }

    lv_label_set_text(wifi_item->sta_lbl, status);
}
