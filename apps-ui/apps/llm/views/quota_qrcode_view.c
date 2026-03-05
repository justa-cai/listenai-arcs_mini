#include "quota_qrcode_view.h"
#include "lisa_ui_llm_base.h"
#include "lisa_ui_fonts.h"

#define TAG "quota_qrcode_view"

typedef struct {
    lv_obj_t obj;
    lv_obj_t *qr_img;
    lv_obj_t *title_label;
    lv_obj_t *message_label;
} quota_qrcode_view_t;

static void quota_qrcode_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void quota_qrcode_view_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_quota_qrcode_view_class = {
    .constructor_cb = quota_qrcode_view_constructor,
    .destructor_cb = quota_qrcode_view_destructor,
    .instance_size = sizeof(quota_qrcode_view_t),
    .base_class = &lv_obj_class
};

static void quota_qrcode_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    quota_qrcode_view_t *view = (quota_qrcode_view_t *)obj;

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(obj, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);

    view->qr_img = lv_img_create(obj);
    lv_obj_set_size(view->qr_img, 148, 148);
    lv_obj_align(view->qr_img, LV_ALIGN_CENTER, 0, 5);

    view->title_label = lv_label_create(obj);
    lv_obj_set_style_text_color(view->title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->title_label, &lv_font_notosans_cs_medium_14, 0);
    lv_obj_set_style_text_align(view->title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(view->title_label, LV_PCT(90));
    lv_label_set_long_mode(view->title_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(view->title_label, "");
    lv_obj_align_to(view->title_label, view->qr_img, LV_ALIGN_OUT_TOP_MID, 0, -25);

    view->message_label = lv_label_create(obj);
    lv_obj_set_style_text_color(view->message_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->message_label, &lv_font_notosans_cs_medium_14, 0);
    lv_obj_set_style_text_align(view->message_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(view->message_label, LV_PCT(100));
    lv_label_set_long_mode(view->message_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(view->message_label, "");
    lv_obj_align_to(view->message_label, view->qr_img, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    LV_TRACE_OBJ_CREATE("finished");
}

static void quota_qrcode_view_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");
    LV_TRACE_OBJ_CREATE("finished");
}

lv_obj_t *lisa_ui_quota_qrcode_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_quota_qrcode_view_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void lisa_ui_quota_qrcode_view_set_title(lv_obj_t *view, const char *title)
{
    quota_qrcode_view_t *v = (quota_qrcode_view_t *)view;
    if (v && v->title_label && title) {
        lv_label_set_text(v->title_label, title);
    }
}

void lisa_ui_quota_qrcode_view_set_message(lv_obj_t *view, const char *message)
{
    quota_qrcode_view_t *v = (quota_qrcode_view_t *)view;
    if (v && v->message_label && message) {
        lv_label_set_text(v->message_label, message);
    }
}

void lisa_ui_quota_qrcode_view_set_qr_image(lv_obj_t *view, const void *img_src)
{
    quota_qrcode_view_t *v = (quota_qrcode_view_t *)view;
    if (v && v->qr_img && img_src) {
        lv_img_set_src(v->qr_img, img_src);
    }
}
