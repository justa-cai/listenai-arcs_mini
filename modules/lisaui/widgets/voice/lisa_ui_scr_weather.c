#include "lvgl.h"

extern const lv_obj_class_t lisa_ui_base_class;

LV_FONT_DECLARE(lv_font_chinese_18);
LV_FONT_DECLARE(lv_font_notosans_cs_medium_18);
LV_FONT_DECLARE(lv_font_notosans_cs_medium_14);
LV_FONT_DECLARE(lv_font_rubik_bold_64);
LV_FONT_DECLARE(lv_font_rubik_bold_64_ascii);

struct lisa_ui_weather {
    lv_obj_t obj;
    lv_obj_t *lbl_location;
    lv_obj_t *lbl_date;
    lv_obj_t *img_weather_icon;
    lv_obj_t *lbl_temp_real;
    lv_obj_t *lbl_temp_range;
    lv_obj_t *lbl_weather_description;
};

typedef struct lisa_ui_weather lisa_ui_weather_t;

const lv_obj_class_t lv_lisa_ui_weather_class = {
    .base_class = &lisa_ui_base_class,
    .width_def = LV_DPI_DEF * 2,
    .height_def = LV_SIZE_CONTENT,
    .instance_size = sizeof(lisa_ui_weather_t)
};

#define MY_CLASS &lv_lisa_ui_weather_class

lv_obj_t *lisa_ui_weather_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);

    lisa_ui_weather_t *ui_weather = (lisa_ui_weather_t *)obj;

    /* 位置标签 */
    ui_weather->lbl_location = lv_label_create(obj);
    lv_label_set_text(ui_weather->lbl_location, "Location");
    lv_obj_set_style_text_color(ui_weather->lbl_location, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_weather->lbl_location, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_align(ui_weather->lbl_location, LV_ALIGN_TOP_LEFT, LV_DPX(40), LV_DPX(0));

    /* 日期标签 */
    ui_weather->lbl_date = lv_label_create(obj);
    lv_label_set_text(ui_weather->lbl_date, "Date");
    lv_obj_set_style_text_color(ui_weather->lbl_date, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_weather->lbl_date, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_align_to(ui_weather->lbl_date, ui_weather->lbl_location, LV_ALIGN_OUT_RIGHT_MID, LV_DPX(10), 0);

    /* 天气图标 */
    ui_weather->img_weather_icon = lv_img_create(obj);
    lv_obj_set_size(ui_weather->img_weather_icon, LV_DPX(72), LV_DPX(72));
    lv_obj_set_style_bg_img_opa(ui_weather->img_weather_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui_weather->img_weather_icon, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_weather->img_weather_icon, LV_DPX(2), LV_PART_MAIN);
    lv_obj_align(ui_weather->img_weather_icon, LV_ALIGN_CENTER, -LV_DPX(100), 0);

    /* 实时温度 */
    ui_weather->lbl_temp_real = lv_label_create(obj);
    lv_label_set_text(ui_weather->lbl_temp_real, "25℃");
    lv_obj_set_style_text_color(ui_weather->lbl_temp_real, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_weather->lbl_temp_real, &lv_font_rubik_bold_64_ascii, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_weather->lbl_temp_real, LV_DPX(2), LV_PART_MAIN);
    lv_obj_align(ui_weather->lbl_temp_real, LV_ALIGN_RIGHT_MID, -LV_DPX(30), -LV_DPX(36));

    /* 温度范围 */
    ui_weather->lbl_temp_range = lv_label_create(obj);
    lv_label_set_text(ui_weather->lbl_temp_range, "25℃~35℃");
    lv_obj_set_style_text_color(ui_weather->lbl_temp_range, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_weather->lbl_temp_range, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_align_to(ui_weather->lbl_temp_range, ui_weather->lbl_temp_real, LV_ALIGN_OUT_BOTTOM_LEFT, LV_DPX(0), LV_DPX(10));

    /* 天气描述 */
    ui_weather->lbl_weather_description = lv_label_create(obj);
    lv_label_set_text_fmt(ui_weather->lbl_weather_description, "%s  %s", "weather", "description");
    lv_obj_set_style_text_color(ui_weather->lbl_weather_description, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_weather->lbl_weather_description, &lv_font_chinese_18, LV_PART_MAIN);

    lv_obj_align_to(ui_weather->lbl_weather_description, ui_weather->lbl_temp_range, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPX(0));

    return obj;
}

int lisa_ui_weather_location_set(lv_obj_t *obj, const char *location)
{
    lisa_ui_weather_t *ui_weather = (lisa_ui_weather_t *)obj;
    lv_label_set_text(ui_weather->lbl_location, location);

    return 0;
}

int lisa_ui_weather_date_set(lv_obj_t *obj, const char *date)
{
    lisa_ui_weather_t *ui_weather = (lisa_ui_weather_t *)obj;
    lv_label_set_text(ui_weather->lbl_date, date);
    return 0;
}

int lisa_ui_weather_icon_set(lv_obj_t *obj, const char *icon_url)
{
    lisa_ui_weather_t *ui_weather = (lisa_ui_weather_t *)obj;
    lv_img_set_src(ui_weather->img_weather_icon, icon_url);

    return 0;
}

int lisa_ui_weather_temp_real_set(lv_obj_t *obj, const char *temp_real)
{
    lisa_ui_weather_t *ui_weather = (lisa_ui_weather_t *)obj;
    lv_label_set_text(ui_weather->lbl_temp_real, temp_real);

    return 0;
}

int lisa_ui_weather_temp_range_set(lv_obj_t *obj, const char *temp_range)
{
    lisa_ui_weather_t *ui_weather = (lisa_ui_weather_t *)obj;
    lv_label_set_text(ui_weather->lbl_temp_range, temp_range);

    return 0;
}

int lisa_ui_weather_description_set(lv_obj_t *obj, const char *desc)
{
    lisa_ui_weather_t *ui_weather = (lisa_ui_weather_t *)obj;
    lv_label_set_text(ui_weather->lbl_weather_description, desc);

    return 0;
}
