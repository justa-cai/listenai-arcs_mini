/**
 * @file info_view.c
 * @brief Info/QR Code page view implementation
 * 
 * Pure UI layer implementation for info/qrcode display page.
 * This view displays QR codes (BLE or network) and informational text.
 */

#define TAG "info_view"

#include "info_view.h"
#include "lisa_ui.h"
#include "lisa_ui_llm_base.h"
#include "lisa_ui_res.h"
#include "lisa_ui_fonts.h"
#include <string.h>


/** Default text displayed when no specific info is set */
#define DEFAULT_INFO_TEXT "Please scan QR code"

/**
 * @brief Info view structure
 * Inherits from lisa_ui_llm_base and adds QR code display elements
 */
struct lisa_ui_info_view {
    lisa_ui_llm_base_t base_obj;       /**< Base object */
    
    /* Content elements */
    lv_obj_t *top_label;               /**< Top instruction text */
    lv_obj_t *qr_img;                  /**< QR code image */
    lv_obj_t *bottom_label;            /**< Bottom information text */
    
    /* QR image descriptor for RGB565 data */
    lv_img_dsc_t qr_img_desc;          /**< Image descriptor */
};

typedef struct lisa_ui_info_view lisa_ui_info_view_t;

static void lisa_ui_info_view_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lisa_ui_info_view_class_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_info_view_class = {
    .base_class = &lisa_ui_llm_base_class,
    .instance_size = sizeof(lisa_ui_info_view_t),
    .constructor_cb = lisa_ui_info_view_class_constructor,
    .destructor_cb = lisa_ui_info_view_class_destructor,
};

static void lisa_ui_info_view_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    (void)class_p;
    lisa_ui_info_view_t *info_view = (lisa_ui_info_view_t *)obj;
    
    // Hide the bar to allow content to start from the very top
    lv_obj_t *bar = lisa_ui_llm_base_bar_get(obj);
    if (bar) {
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }
    
    lv_obj_t *container = lisa_ui_llm_base_container_get(obj);
    if (NULL == container) {
        LISA_UI_LOGE("Failed to get base container");
        return;
    }
    
    // Create QR code image in center
    info_view->qr_img = lv_img_create(container);
#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    lv_obj_set_size(info_view->qr_img, 160, 160);
    lv_obj_align(info_view->qr_img, LV_ALIGN_CENTER, -80, 0);
#else
    lv_obj_set_size(info_view->qr_img, 148, 148);
    lv_obj_align(info_view->qr_img, LV_ALIGN_CENTER, 0, 5);
#endif
    
    // Create top label above QR code
    info_view->top_label = lv_label_create(container);
    lv_label_set_text(info_view->top_label, "");
    lv_obj_set_style_text_color(info_view->top_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(info_view->top_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(info_view->top_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(info_view->top_label, LV_PCT(90));
    lv_label_set_long_mode(info_view->top_label, LV_LABEL_LONG_WRAP);
    lv_obj_align_to(info_view->top_label, info_view->qr_img, LV_ALIGN_OUT_TOP_MID, 0, -25);
    
    // Create bottom label below QR code
    info_view->bottom_label = lv_label_create(container);
    lv_label_set_text(info_view->bottom_label, "");
    lv_obj_set_style_text_color(info_view->bottom_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(info_view->bottom_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(info_view->bottom_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(info_view->bottom_label, LV_PCT(90));
    lv_label_set_long_mode(info_view->bottom_label, LV_LABEL_LONG_WRAP);
    lv_obj_align_to(info_view->bottom_label, info_view->qr_img, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

static void lisa_ui_info_view_class_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    (void)class_p;
    (void)obj;
    // Child objects auto-deleted by LVGL
}

// ===================== Public API Implementation =====================

lv_obj_t *lisa_ui_info_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_info_view_class, parent);
    lv_obj_class_init_obj(obj);
    
    // Set page title
    lisa_ui_llm_base_set_title(obj, "Info");
    
    return obj;
}

void lisa_ui_info_view_set_top_text(lv_obj_t *obj, const char *text)
{
    if (!obj || !lv_obj_has_class(obj, &lisa_ui_info_view_class)) {
        return;
    }
    
    lisa_ui_info_view_t *info_view = (lisa_ui_info_view_t *)obj;
    if (info_view->top_label) {
        lv_label_set_text(info_view->top_label, text ? text : DEFAULT_INFO_TEXT);
    }
}

void lisa_ui_info_view_set_bottom_text(lv_obj_t *obj, const char *text)
{
    if (!obj || !lv_obj_has_class(obj, &lisa_ui_info_view_class)) {
        return;
    }
    
    lisa_ui_info_view_t *info_view = (lisa_ui_info_view_t *)obj;
    if (info_view->bottom_label) {
        lv_label_set_text(info_view->bottom_label, text ? text : "");
    }
}

void lisa_ui_info_view_set_qr_image(lv_obj_t *obj, const void *src)
{
    if (!obj || !lv_obj_has_class(obj, &lisa_ui_info_view_class)) {
        return;
    }
    
    lisa_ui_info_view_t *info_view = (lisa_ui_info_view_t *)obj;
    if (info_view->qr_img && src) {
        lv_img_set_src(info_view->qr_img, src);
    }
}

lv_obj_t *lisa_ui_info_view_get_qr_image(lv_obj_t *obj)
{
    if (!obj || !lv_obj_has_class(obj, &lisa_ui_info_view_class)) {
        return NULL;
    }
    
    lisa_ui_info_view_t *info_view = (lisa_ui_info_view_t *)obj;
    return info_view->qr_img;
}

void lisa_ui_info_view_set_qr_rgb565(lv_obj_t *obj, const uint16_t *data, uint16_t width, uint16_t height, uint32_t data_size)
{
    if (!obj || !lv_obj_has_class(obj, &lisa_ui_info_view_class) || !data) {
        return;
    }
    
    lisa_ui_info_view_t *info_view = (lisa_ui_info_view_t *)obj;
    if (!info_view->qr_img) {
        return;
    }
    
    // Setup image descriptor for RGB565 format
    memset(&info_view->qr_img_desc, 0, sizeof(lv_img_dsc_t));
    info_view->qr_img_desc.header.always_zero = 0;
    info_view->qr_img_desc.header.w = width;
    info_view->qr_img_desc.header.h = height;
    info_view->qr_img_desc.header.cf = LV_IMG_CF_TRUE_COLOR;  // RGB565 format
    info_view->qr_img_desc.data_size = data_size;
    info_view->qr_img_desc.data = (const uint8_t *)data;
    
    // Set image source to descriptor
    lv_img_set_src(info_view->qr_img, &info_view->qr_img_desc);
}
