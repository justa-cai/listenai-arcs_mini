/**
 * @file app_template.c
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-01-22
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_template.h"
#include "lisaui_app_common.h"
#include "assets/assets_res.h"

static const char *TAG = "app_template";
static lv_obj_t *g_app_template = NULL;

#if CONFIG_LISAUI_APP_TEMPLATE_MACRO_ENABLE

// 声明[模板]应用需要实现的接口
LISAUI_DECLARE_APP_FUNC(template, create, destroy, enter, exit, get_view);
// 定义[模板]应用
LISAUI_DEFINE_APP(template, UI_APP_ID_TEMPLATE, "模板", "Template", &icon_img_app_store_png, {});

// App 模板宏原型在 common/lisaui_app_manager.h 中定义
// 定义[模板]应用功能实现, 用于创建、销毁、进入、退出、获取页面
#define LISAUI_APP_ENTITY_TEMPLATE                                                                                     \
    {                                                                                                                  \
        if (g_app_template != NULL) {                                                                                  \
            return LISAUI_ERR_OK;                                                                                      \
        }                                                                                                              \
        g_app_template = lv_obj_create(parent);                                                                        \
        _lisaui_set_style_container(g_app_template, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);        \
                                                                                                                       \
        lv_obj_t *label = lv_label_create(g_app_template);                                                             \
        lv_label_set_text(label, "Hello LisaUI!");                                                                     \
        lv_obj_set_style_text_font(label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);                       \
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);                   \
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);                                                                    \
                                                                                                                       \
        return LISAUI_ERR_OK;                                                                                          \
    }
lisaui_err_t LISAUI_DEFINE_APP_FUNC(template, create, void *parent, {LISAUI_APP_ENTITY_TEMPLATE});
lisaui_err_t LISAUI_DEFINE_APP_FUNC(template, destroy, void, {
    LVGL_OBJ_SAFE_DEL(g_app_template);
    return LISAUI_ERR_OK;
});
lisaui_err_t LISAUI_DEFINE_APP_FUNC(template, enter, void, { return LISAUI_ERR_OK; });
lisaui_err_t LISAUI_DEFINE_APP_FUNC(template, exit, void, { return LISAUI_ERR_OK; });
void *LISAUI_DEFINE_APP_FUNC(template, get_view, void, { return g_app_template; });

#else

lisaui_err_t app_template_create(void *parent)
{
    if (g_app_template != NULL) {
        return LISAUI_ERR_OK;
    }
    g_app_template = lv_obj_create(parent);
    _lisaui_set_style_container(g_app_template, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);

    lv_obj_t *label = lv_label_create(g_app_template);
    lv_label_set_text(label, "Hello LisaUI!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    LISAUI_LOGI(TAG, "[%d:%s] create\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_template_destroy(void)
{
    LVGL_OBJ_SAFE_DEL(g_app_template);

    LISAUI_LOGI(TAG, "[%d:%s] destroy\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_template_enter(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] enter\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

lisaui_err_t app_template_exit(void)
{
    LISAUI_LOGI(TAG, "[%d:%s] exit\n", __LINE__, __func__);
    return LISAUI_ERR_OK;
}

void *app_template_get_view(void)
{
    return g_app_template;
}

static struct app_icon_t app_icon_res_template = {
    .title = "模板",
    // .icon_width = LV_SIZE_CONTENT,
    // .icon_height = LV_SIZE_CONTENT,
    .icon = &icon_img_app_store_png,
    .zoom = APP_ICON_ZOOM(0),
};

lisaui_app_t app_template = {
    .create = app_template_create,
    .destroy = app_template_destroy,
    .enter = app_template_enter,
    .exit = app_template_exit,

    .get_app_view = app_template_get_view,
    .info =
        {
            .name = "Template",
            .package_name = "com.listenai.lisaui.template",
            .id = UI_APP_ID_TEMPLATE,
        },
    .icon = &app_icon_res_template,
};

lisaui_err_t app_template_init(void)
{
    lisaui_app_register(&app_template);
    return LISAUI_ERR_OK;
}

LISAUI_REGISTER_APP(template, &app_template, app_template_init);

#endif