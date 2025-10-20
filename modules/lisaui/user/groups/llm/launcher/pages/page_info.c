#include <stdint.h>
#include <string.h>

#include "ebus/ebus.h"
#include "lvgl.h"

#include "platform.h"
#include "lisaui_common.h"
#include "lisaui_stack_page.h"
#include "lisaui_group.h"
#include "lisaui_manager.h"

#include "assets/assets_res.h"
#include "launcher_pages.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_assets.h"
#include "user_groups.h"
#include "lisaui_log.h"
#include "lisaui_user_data.h"
#include "lisa_display.h"

#include "../group_launcher.h"

#define TAG "launcher.page.info"
LV_IMG_DECLARE(ble_qr);
// 全局状态变量
static bool g_info_page_active = false;

// 函数声明
static void auto_return_timer_cb(lv_timer_t *timer);
bool is_info_page_active(void);
void set_info_page_active(bool active);

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *label;
    lv_obj_t *g_ble_qr_img;
    lv_timer_t *auto_return_timer;
    ebus_chn_t *ebus_ch_base_event;
} page_view_t;

static lisaui_page_t *create(lisaui_page_t *page)
{
    LISAUI_LOGI(TAG, "create info page");

    page_view_t *view = NULL;

    view = lisaui_malloc(sizeof(page_view_t));
    if (!view) {
        LISAUI_LOGE(TAG, "failed to allocate page view");
        return NULL;
    }
    memset(view, 0, sizeof(page_view_t));

    // 创建屏幕对象
    view->screen = lv_obj_create(NULL);
    if (!view->screen) {
        LISAUI_LOGE(TAG, "failed to create screen object");
        lisaui_free(view);
        return NULL;
    }

    // 验证创建的对象
    if (!lv_obj_is_valid(view->screen)) {
        LISAUI_LOGE(TAG, "created screen object is invalid");
        lisaui_free(view);
        return NULL;
    }

    // 设置屏幕为全屏黑色背景
    lv_obj_set_size(view->screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(view->screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(view->screen, 0, 0);

    view->g_ble_qr_img = lv_img_create(view->screen);
    lv_img_set_src(view->g_ble_qr_img, &ble_qr);
    lv_obj_align(view->g_ble_qr_img, LV_ALIGN_CENTER, 0, 0);

    // // 创建文本标签
    view->label = lv_label_create(view->screen);
    if (!view->label) {
        LISAUI_LOGE(TAG, "failed to create label object");
        lv_obj_del(view->screen);
        lisaui_free(view);
        return NULL;
    }

    // 设置标签属性
    lv_label_set_text(view->label, "扫码快速配置");
    lv_obj_set_style_text_color(view->label, lv_color_white(), 0);
    lv_obj_align_to(view->label, view->g_ble_qr_img, LV_ALIGN_OUT_BOTTOM_MID, -20, 0);
    lv_obj_set_style_text_font(view->label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 初始化定时器为NULL
    view->auto_return_timer = NULL;
    
    view->ebus_ch_base_event = ebus_chn_bind(LISAUI_EBUS_NAME, LISAUI_EBUS_CH_BASE_EVENT_NAME);
    if (view->ebus_ch_base_event != NULL) {
        ebus_message_subscribe( view->ebus_ch_base_event ,
                        EBUS_SUBSCRIBER_TYPE_SYNC, 
                        LISAUI_EBUS_CH_EVENT_U2M_SETTING_HOME_UPDATE,
                        NULL, 
                        view);

    }
    page->view = view;
    LISAUI_LOGI(TAG, "info page created successfully %p", view->screen);
    //
    LISAUI_LOGI(TAG, "info page created successfully!!! %p", view->screen);
    return page;
}
static lisaui_err_t destroy(lisaui_page_t *page)
{
    LISAUI_LOGI(TAG, "destroy info page");

    if (page && page->view) {
        page_view_t *view = (page_view_t *)page->view;

        // 清理定时器
        if (view->auto_return_timer) {
            lv_timer_del(view->auto_return_timer);
            view->auto_return_timer = NULL;
        }

        if (view->screen) {
            lv_obj_del(view->screen);
        }
        lisaui_free(view);
        page->view = NULL;
    }

    return LISAUI_ERR_OK;
}
// 自动返回定时器回调函数
static void auto_return_timer_cb(lv_timer_t *timer)
{
    LISAUI_LOGI(TAG, "Auto return timer expired, returning to home page");
    
    // 重置状态
    g_info_page_active = false;
    
    // 返回主页面
    lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                               LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY, 0);
   
    // 定时器会被自动删除，因为是一次性定时器
}

void _ui_screen_change(lv_obj_t *target, lv_scr_load_anim_t fademode, int spd, int delay)
{
    lv_scr_load_anim(target, fademode, spd, delay, false);
}

static lisaui_err_t show(lisaui_page_t *page)
{
    LISAUI_LOGI(TAG, "show info page ,%p", page);

    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for show");
        return LISAUI_ERR_INVALID_PARAM;
    }

    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (!view->screen) {
        LISAUI_LOGE(TAG, "Screen object is NULL");
        return LISAUI_ERR_INVALID_PARAM;
    }

    // 加载屏幕前进行额外验证
    if (!lv_obj_is_valid(view->screen)) {
        LISAUI_LOGE(TAG, "Screen object is invalid before loading");
        return LISAUI_ERR_INVALID_PARAM;
    }

    // 设置页面激活状态
    set_info_page_active(true);

    // 使用延时加载避免竞态条件
    lv_scr_load_anim(view->screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    
    // 启动10秒自动返回定时器
    if (!view->auto_return_timer) {
        view->auto_return_timer = lv_timer_create(auto_return_timer_cb, 10000, NULL);
        lv_timer_set_repeat_count(view->auto_return_timer, 1); // 一次性定时器
        LISAUI_LOGI(TAG, "Auto return timer started (10s)");
    }
    
    LISAUI_LOGI(TAG, "info page shown");

    return LISAUI_ERR_OK;
}
static lisaui_err_t close(lisaui_page_t *page)
{
    LISAUI_LOGI(TAG, "close info page");

    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for close");
        return LISAUI_ERR_INVALID_PARAM;
    }

    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }

    // 清理定时器并重置状态
    if (view->auto_return_timer) {
        lv_timer_del(view->auto_return_timer);
        view->auto_return_timer = NULL;
        LISAUI_LOGI(TAG, "Auto return timer deleted");
    }
    
    // 重置页面状态
    g_info_page_active = false;
    ebus_message_pub(view->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_SETTING_HOME_UPDATE, NULL, 0);
    // 页面关闭时不需要特殊处理，框架会自动切换屏幕
    LISAUI_LOGI(TAG, "info page closed");

    return LISAUI_ERR_OK;
}

static lisaui_err_t update_data(lisaui_page_t *page, void *data)
{
    // 暂时不需要数据更新功能
    return LISAUI_ERR_OK;
}

lisaui_page_t page_info = {
    .cname = "page_info",
    .group_index = LISAUI_GROUP_INDEX_LAUNCHER,
    .page_index = LISAUI_GROUP_LAUNCHER_PAGE_INDEX_INFO,
    .view = NULL,
    .page_data = NULL,
    .attribute =
        {
            .type = LISAUI_PAGE_TYPE_DATA_PAGE,
        },
    .flags = 0,
    .create = create,
    .destroy = destroy,
    .show = show,
    .close = close,
    .update_data = update_data,
};

// 提供给外部模块的接口函数
bool is_info_page_active(void)
{
    return g_info_page_active;
}

void set_info_page_active(bool active)
{
    g_info_page_active = active;
    LISAUI_LOGI(TAG, "Info page active state set to: %d", active);
}

LISAUI_PAGE_EXPORT(page_info);