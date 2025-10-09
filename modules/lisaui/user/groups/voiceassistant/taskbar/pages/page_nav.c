#include <stdint.h>
#include "lvgl.h"
#include "platform.h"
#include "lisaui_common.h"
#include "lisaui_stack_page.h"
#include "lisaui_group.h"
#include "lisaui_manager.h"

#include "assets/assets_res.h"
#include "../group_taskbar.h"
#include "setting/group_setting.h"
#include "taskbar_pages.h"
#include "lisaui_log.h"
#include "user_groups.h"
#include "lisa_ui_sys_bar.h"

#define TAG "taskbar.page.nav"

typedef struct {
    lv_obj_t *taskbar;

} page_view_t;

static void btn_nav_click_event_cb(void *user_data)
{
    uint8_t is_home = 0;
    lisaui_group_t *group;

    /*check if current page is launcher main page,goto setting group*/
    group = lisaui_manager_get_current_group();

    if (group != NULL) {

        if ((group->info.type == LISAUI_GROUP_TYPE_LAUNCHER) && (group->current_page != NULL) &&
            (group->current_page->attribute.type == LISAUI_PAGE_TYPE_PRIMARY_PAGE)) {

            lisaui_manager_group_enter(LISAUI_GROUP_INDEX_SETTING, GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                                       LISAUI_GROUP_SETTING_PAGE_INDEX_PRIMARY, 0);
            return;
        }
    }

    /*nav back*/
    lisaui_manager_nav_back();

    LISAUI_LOGI(TAG, "taskbar nav page btn_nav_click_event_cb, is_home:%d,user_data:%p\n", is_home, user_data);
}

static int manager_event_cb(uint32_t events, lisaui_group_t *sender, void *user_data)
{

    uint8_t is_home = 0;

    LISAUI_LOGI(TAG, "manager_event_cb,events:0x%x,sender:%p,user_data:%p", events, sender, user_data);
    if (events & LISAUI_MANAGER_EVENT_GROUP_SWITCH_PAGE) {

        if (sender->info.id == LISAUI_GROUP_INDEX_LAUNCHER) {
            if (sender->current_page != NULL) {
                if (sender->current_page->attribute.type == LISAUI_PAGE_TYPE_PRIMARY_PAGE) {
                    is_home = 1;
                }
            }
        }
        if (is_home) {
            lisa_ui_sys_bar_btn_nav_img_set(LV_SYMBOL_SETTINGS);
        } else {
            lisa_ui_sys_bar_btn_nav_img_set(LV_SYMBOL_LEFT);
        }
    }

    return LISAUI_ERR_OK;
}

static lisaui_page_t *create(lisaui_page_t *page)
{

    page_view_t *view = NULL;

    view = lisaui_malloc(sizeof(page_view_t));
    if (view == NULL) {
        LISAUI_LOGE(TAG, "[%s %d]no memory!", __FUNCTION__, __LINE__);
        goto _ERR;
    }

    view->taskbar = lisa_ui_sys_bar_get();
    lisa_ui_sys_bar_btn_nav_img_set(LV_SYMBOL_SETTINGS);
    lisa_ui_sys_bar_btn_nav_click_event_cb_set(btn_nav_click_event_cb, view->taskbar);
    lisaui_manager_add_callback(LISAUI_MANAGER_EVENT_GROUP_SWITCH_PAGE, manager_event_cb, page);
    page->view = view;
    LISAUI_LOGI(TAG, "Create taskbar nav page:%p", page);
    return page;

_ERR:

    if (NULL != view) {
        lisaui_free(view);
    }

    return NULL;
}

static lisaui_err_t destroy(lisaui_page_t *page)
{
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for destroy");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (page->page_data) {
        lisaui_free(page->page_data);
        page->page_data = NULL;
    }

    LISAUI_LOGI(TAG, "Destroy %s page", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t show(lisaui_page_t *page)
{

    if (!page) {

        LISAUI_LOGE(TAG, "Invalid page pointer for show");
        return LISAUI_ERR_INVALID_PARAM;
    }

    LISAUI_LOGI(TAG, "Show %s page", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t close(lisaui_page_t *page)
{
    if (!page) {

        LISAUI_LOGE(TAG, "Invalid page pointer for close");
        return LISAUI_ERR_INVALID_PARAM;
    }

    LISAUI_LOGI(TAG, "Close %s page", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t update_data(lisaui_page_t *page, void *data)
{

    launcher_page_update_parameter_t *parameter = (launcher_page_update_parameter_t *)data;
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for update");
        return LISAUI_ERR_INVALID_PARAM;
    }

    LISAUI_LOGI(TAG, "Update %s page data", page->cname);
    return LISAUI_ERR_OK;
}

static const lisaui_page_t taskbar_nav_page = {
    .group_index = LISAUI_GROUP_INDEX_TASKBAR,
    .page_index = LISAUI_GROUP_TASKBAR_PAGE_INDEX_NAV,
    .cname = "taskbar.nav",
    .view = NULL,
    .page_data = NULL,
    .attribute =
        {
            .type = LISAUI_PAGE_TYPE_PRIMARY_PAGE,
        },
    .create = create,
    .destroy = destroy,
    .show = show,
    .close = close,
    .update_data = update_data,
};

LISAUI_PAGE_EXPORT(taskbar_nav_page);
