#include <stdint.h>

#include "ebus/ebus.h"
#include "lvgl.h"

#include "platform.h"
#include "lisaui_common.h"
#include "lisaui_stack_page.h"
#include "lisaui_group.h"
#include "lisaui_manager.h"

#include "assets/assets_res.h"
#include "launcher_pages.h"
#include "lisa_ui_scr_ai_human_main.h"
#include "lisa_ui_scr_ai_human_commu.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_assets.h"
#include "user_groups.h"
#include "lisaui_log.h"
#include "lisaui_user_data.h"

#include "../group_launcher.h"

#define TAG "launcher.page.main"

/**
 * 页面视图结构体
 * 遵循MVC架构，视图仅负责UI渲染
 */

typedef struct {
    const char *name;
    const void *img_path;
} role_images_t;

typedef struct {
    lv_obj_t *inter; /**< 组网格视图组件 */
    ebus_chn_t *ebus_ch_base_event;
} page_view_t;

static void commu_page_enter(page_view_t *view, const char *img_name,bool is_push_stack)
{

    uint8_t role_founded = 0;

    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        for (int i = 0; i < _userdata->roles.roles_count; i++) {
            if (0 == strcmp(img_name, _userdata->roles.roles[i].name)) {
                _userdata->roles.role_idx = i;
                role_founded = 1;
                break;
            }
        }
    }

    if (role_founded) {

        LISAUI_LOGI(TAG, "imgs_click_event_cb enter commu page\n");

        lisaui_manager_group_enter( LISAUI_GROUP_INDEX_LAUNCHER, 
                                    GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                                    LISAUI_GROUP_LAUNCHER_PAGE_INDEX_COMMU, 
                                    is_push_stack ? LISAUI_PAGE_SAVE_TO_HISTORY : 0);
        ebus_message_pub(view->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_INTER_ROLE_UPDATE, NULL, 0);
    }
}

static void imgs_click_event_cb(void *user_data, const char *img_name)
{
    page_view_t *view = (page_view_t *)user_data;
    LISAUI_LOGI(TAG, "imgs_click_event_cb, img_name:%s\n", img_name);
    commu_page_enter(view, img_name, true);
}

static int update_roles_state(page_view_t *view)
{
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        if (_userdata->roles.request_state == 1) {

            for (int i = 0; i < _userdata->roles.roles_count; i++) {

                lisa_ui_scr_ai_human_main_imgs_add(view->inter, _userdata->roles.roles[i].img_main,
                                                   _userdata->roles.roles[i].name);
            }

            lisa_ui_scr_ai_human_main_imgs_click_event_cb_set(view->inter, imgs_click_event_cb, view);
        }
    }
}

static int event_inter_state_handler(ebus_chn_t *chn, uint32_t code,void *message, uint32_t msg_size, void *user_data)
{
    page_view_t *view = (page_view_t *)user_data;
    uint32_t current_page_index;

    if (view == NULL) {
        return -1;
    }
    LISAUI_LOGI(TAG, "event_inter_state_handler, code:%d", code);
    switch (code) {
    case LISAUI_EBUS_CH_EVENT_M2U_ROLES_UPDATE:
        update_roles_state(view);
        break;
    case LISAUI_EBUS_CH_EVENT_M2U_INTER_WAKEUP: {
        const char *role_name = NULL;
        LISAUI_USERDATA_WITH_LOCK(_userdata)
        {
            role_name = _userdata->roles.roles[_userdata->roles.role_idx].name;
        }
        commu_page_enter(
            view, role_name,
            (lisaui_manager_get_current_group()->current_page->attribute.type == LISAUI_PAGE_TYPE_PRIMARY_PAGE)
                ? true
                : false);
    } break;
    case LISAUI_EBUS_CH_EVENT_M2U_INTER_END:
        // lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
        //                            LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY, 0);

        break;
    default:
        break;
    }

    return 0;
}

static lisaui_page_t *create(lisaui_page_t *page)
{
    page_view_t *view = NULL;

    // 分配视图内存
    view = lisaui_malloc(sizeof(page_view_t));
    if (view == NULL) {

        LISAUI_LOGE(TAG, "[%s %d]no memory!", __FUNCTION__, __LINE__);
        goto _ERR;
    }

    view->inter = lisa_ui_scr_ai_human_main_create(NULL);
    lisa_ui_scr_base_title_set(view->inter, "选择人物");
    update_roles_state(view);
    view->ebus_ch_base_event = ebus_chn_bind(LISAUI_EBUS_NAME, LISAUI_EBUS_CH_BASE_EVENT_NAME);
    if (view->ebus_ch_base_event != NULL) {
        ebus_message_subscribe( view->ebus_ch_base_event, 
                                EBUS_SUBSCRIBER_TYPE_SYNC, 
                                LISAUI_EBUS_CH_EVENT_M2U_ROLES_UPDATE,
                                event_inter_state_handler, 
                                view);
        ebus_message_subscribe( view->ebus_ch_base_event, 
                                EBUS_SUBSCRIBER_TYPE_SYNC, 
                                LISAUI_EBUS_CH_EVENT_M2U_INTER_WAKEUP,
                                event_inter_state_handler, 
                                view);
        // ebus_message_subscribe( view->ebus_ch_base_event, 
        //                         EBUS_SUBSCRIBER_TYPE_SYNC, 
        //                         LISAUI_EBUS_CH_EVENT_M2U_INTER_END,
        //                         event_inter_state_handler, 
        //                         view);
    }

    page->view = view;

    LISAUI_LOGI(TAG, "Luncher main page create success:%p", page);
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

    if (page->view) {
        page_view_t *view = (page_view_t *)page->view;
        ebus_message_unsubscribe(view->ebus_ch_base_event, event_inter_state_handler);
        if (view->inter) {
            lisa_ui_scr_base_del(view->inter);
            view->inter = NULL;
        }

        lisaui_free(view);
        page->view = NULL;
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

    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (view->inter) {
        lisa_ui_scr_base_show(view->inter);
        lisa_ui_scr_base_title_set(view->inter, "选择人物");
    }
    ebus_message_pub(view->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_INTER_ROLE_EXIT, NULL, 0);
    LISAUI_LOGI(TAG, "Show %s page", page->cname);
    return LISAUI_ERR_OK;
}

/**
 * @brief 关闭页面
 * @param page 页面对象
 * @return 错误码
 */
static lisaui_err_t close(lisaui_page_t *page)
{
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for close");
        return LISAUI_ERR_INVALID_PARAM;
    }
    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page %s view not created", page->cname);
        return LISAUI_ERR_INVALID_PARAM;
    }

    lisa_ui_scr_base_hide(view->inter);

    LISAUI_LOGI(TAG, "Close %s page", page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t update_data(lisaui_page_t *page, void *data)
{
    launcher_page_update_parameter_t *parameter = (launcher_page_update_parameter_t *)data;
    if (!page || !data) {
        LISAUI_LOGE(TAG, "Invalid parameters for update");
        return LISAUI_ERR_INVALID_PARAM;
    }

    page_view_t *view = (page_view_t *)page->view;
    if (!view || !view->inter) {
        LISAUI_LOGE(TAG, "Page %s view not created or inter is NULL", page->cname);
        return LISAUI_ERR_INVALID_PARAM;
    }

    LISAUI_LOGI(TAG, "Update %s page", page->cname);
    return LISAUI_ERR_OK;
}

static const lisaui_page_t luancher_main_page = {
    .group_index = LISAUI_GROUP_INDEX_LAUNCHER,
    .page_index = LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY,
    .cname = "launcher.main",
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

LISAUI_PAGE_EXPORT(luancher_main_page);
