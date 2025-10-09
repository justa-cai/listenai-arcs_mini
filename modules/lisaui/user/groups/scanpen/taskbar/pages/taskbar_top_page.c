#include <stdint.h>
#include "lvgl.h"
#include "platform.h"
#include "lisaui_common.h"
#include "lisaui_stack_page.h"
#include "lisaui_group.h"
#include "lisaui_manager.h"

#include "assets/assets_res.h"
#include "../group_taskbar.h"
#include "taskbar_pages.h"
#include "../widgets/lv_top_taskbar.h"
#include "lisaui_log.h"
#include "user_groups.h"

#define TAG "taskbar.page.main"

typedef struct{
    lv_obj_t *display;
    lv_obj_t *taskbar;
}page_view_t;

static void event_handler_display_panel(lv_event_t *e){
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_taskbar_event_data_t *event_params = lv_event_get_param(e);
    lv_obj_t *target = lv_event_get_target(e);

    if (event_code == LV_TASKBAR_EVENT_BUTTON_CLICKED) {
        lisaui_group_t *group = lisaui_manager_get_current_group();
        if(group == NULL){
            return;
        }
        
        if(event_params->btn_type == LV_TASKBAR_BTN_CLOSE){
            lisaui_manager_send_event(group,LISAUI_MANAGER_EVENT_SYS_BUTTON_CLOSE_CLICKED);
        }
        else if(event_params->btn_type == LV_TASKBAR_BTN_HOME){
            lisaui_manager_send_event(group,LISAUI_MANAGER_EVENT_SYS_BUTTON_HOME_CLICKED);
        }
        else if(event_params->btn_type == LV_TASKBAR_BTN_BACK){
            lisaui_manager_send_event(group,LISAUI_MANAGER_EVENT_SYS_BUTTON_BACK_CLICKED);
            // lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER,GROUP_ENTER_PAGE_METHOD_POP_STACK_TOP,0);   
        }
        
    }
}

static lisaui_page_t *create(lisaui_page_t *page){

    page_view_t *view = NULL;

    view = lisaui_malloc(sizeof(page_view_t));
    if(view == NULL){
        LISAUI_LOGE(TAG,"[%s %d]no memory!",__FUNCTION__,__LINE__);
        goto _ERR;
    }
    
    view->display = lv_obj_create(lv_layer_sys()); // 放置在系统图层
    lv_obj_set_size(view->display, LV_PCT(100), LV_DPX(LISAUI_STATUS_BAR_HEIGHT));
    lisaui_common_set_style_container(view->display, lv_color_hex(0x000000), 0, lv_color_hex(0x000000), 0, 0);
    
    view->taskbar = lv_taskbar_create(view->display);
    lv_taskbar_set_event_cb(view->taskbar, event_handler_display_panel, NULL);
   
    page->view = view;
    LISAUI_LOGI(TAG,"Create page:%p",page);
    return page;

_ERR:

    if(NULL != view){
        lisaui_free(view);
    }

    return NULL;
}

static lisaui_err_t destroy(lisaui_page_t *page){
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for destroy");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    /* 释放视图资源 */
    if (page->view) {
        page_view_t *view = (page_view_t *)page->view;
        
        /* 销毁所有UI对象 */
        if (view->display) {
            lv_obj_del(view->display);
            view->display = NULL;
        }
        /* 释放视图结构体 */
        lisaui_free(view);
        page->view = NULL;
    }
    
    /* 释放页面私有数据（如果有的话） */
    if (page->page_data) {
        lisaui_free(page->page_data);
        page->page_data = NULL;
    }
    
    LISAUI_LOGI(TAG, "Page main destroyed");
    return LISAUI_ERR_OK;
}

static lisaui_err_t show(lisaui_page_t *page){

    LISAUI_LOGI(TAG,"Show page:%p",page);
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for show");
        return LISAUI_ERR_INVALID_PARAM;
    }

    LISAUI_LOGI(TAG, "Page main showed");
    return LISAUI_ERR_OK;
}

static lisaui_err_t close(lisaui_page_t *page){
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for close");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    /* 隐藏页面主画面 */
    if (view->display) {
        lv_obj_add_flag(view->display, LV_OBJ_FLAG_HIDDEN);
    }
    
    LISAUI_LOGI(TAG, "Page main closed");
    return LISAUI_ERR_OK;
}

static lisaui_err_t update_data(lisaui_page_t *page,void *data){
    
    launcher_page_update_parameter_t *parameter = (launcher_page_update_parameter_t*)data;
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for update");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    /* 在这里实现页面内容的更新逻辑 */
    /* 比如更新图标、刷新数据等 */


    LISAUI_LOGI(TAG, "Page main updated");
    return LISAUI_ERR_OK;
}

static const lisaui_page_t taskbar_main_page = {
    .group_index = LISAUI_GROUP_INDEX_TASKBAR,
    .page_index = LISAUI_GROUP_TASKBAR_PAGE_INDEX_MAIN,
    .cname = "taskbar.top",
    .view = NULL,
    .page_data = NULL,
    .attribute = 0,
    .create = create,
    .destroy = destroy,
    .show = show,
    .close = close,
    .update_data = update_data,
};

LISAUI_PAGE_EXPORT(taskbar_main_page);
