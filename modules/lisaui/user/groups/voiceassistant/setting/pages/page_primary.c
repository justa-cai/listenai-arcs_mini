#include <stdint.h>
#include "lvgl.h"
#include "platform.h"
#include "lisaui_common.h"
#include "assets/assets_res.h"
#include "lisaui_stack_page.h"
#include "lisaui_group.h"
#include "lisaui_manager.h"

#include "../group_setting.h"
#include "setting_pages.h"
#include "lisaui_log.h"
#include "user_groups.h"
#include "lisa_ui_assets.h"

#include "lisa_ui_scr_setting_main.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_scr_setting_net.h"
#include "lisa_ui_scr_setting_common.h"
#include "lisa_ui_scr_setting_wakeup.h"

#include "page_item.h"

#define TAG "setting.page.main"

typedef struct{
    lv_obj_t *scr;
}page_view_t;

static void setting_item_click_cb(const char *name, void *user_data)
{
    LISAUI_LOGI(TAG, "Setting item click:%s", name);
    page_item_data_t item_data;
    lisaui_group_t *group = lisaui_manager_get_current_group();
    if(group == NULL){
        LISAUI_LOGE(TAG,"[%s %d]Get current group is null!",__FUNCTION__,__LINE__);
        return;
    }

    for(int i = 0; i < SETTING_ITEM_MAX_NUMBER; i++){
        if(0 == strcmp(name,setting_items[i]->name)){
            item_data.item_index = i;
            lisaui_manager_group_enter(group->info.id,
                GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                LISAUI_GROUP_SETTING_PAGE_INDEX_ITEM,
                LISAUI_PAGE_SAVE_TO_HISTORY);
            group->current_page->update_data(group->current_page,&item_data);
            break;
        }
    }
}

static lisaui_page_t *create(lisaui_page_t *page){

    page_view_t *view = NULL;

    view = lisaui_malloc(sizeof(page_view_t));
    if(view == NULL){
        LISAUI_LOGE(TAG,"Create %s page view failed,no memory!",page->cname,__FUNCTION__,__LINE__);
        goto _ERR;
    }

    view->scr = lisa_ui_setting_create(NULL);
    lisa_ui_scr_base_title_set(view->scr, "设置");
    for (int i = 0; i < SETTING_ITEM_MAX_NUMBER; i++) {
    
        lisa_ui_setting_item_add(view->scr, setting_items[i]->name, setting_items[i]->icon);
    }

    lisa_ui_setting_item_click_cb_set(view->scr, setting_item_click_cb, NULL);
    page->view = view;
    LISAUI_LOGI(TAG,"Setting main page create success:%p",page);

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
    
    if (page->view) {
        
        page_view_t *view = (page_view_t *)page->view;
        if (view->scr) {

            lisa_ui_scr_base_del(view->scr);
            view->scr = NULL;
        }

        lisaui_free(view);
        page->view = NULL;
    }
    
    if (page->page_data) {
        lisaui_free(page->page_data);
        page->page_data = NULL;
    }
    
    LISAUI_LOGI(TAG, "Destroy %s page",page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t show(lisaui_page_t *page){

    LISAUI_LOGI(TAG,"Show page:%p",page);
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for show");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }

    /* 显示页面主画面 */
    if (view->scr) {
        lisa_ui_scr_base_show(view->scr);
    }
    
    LISAUI_LOGI(TAG, "Show %s page",page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t close(lisaui_page_t *page){
    if (!page) {
    
        LISAUI_LOGE(TAG, "Invalid page pointer for close");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
    
        LISAUI_LOGE(TAG, "Page %s view not created",page->cname);
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    if (view->scr) {
    
        lisa_ui_scr_base_hide(view->scr);
    }
    
    LISAUI_LOGI(TAG, "Setting main page closed");
    return LISAUI_ERR_OK;
}

static lisaui_err_t update_data(lisaui_page_t *page,void *data){
    
    LISAUI_LOGI(TAG, "Update %s page data",page->cname);
    return LISAUI_ERR_OK;
}

static const lisaui_page_t setting_main_page = {
    .group_index = LISAUI_GROUP_INDEX_SETTING,
    .page_index = LISAUI_GROUP_SETTING_PAGE_INDEX_PRIMARY,
    .cname = "setting.main",
    .view = NULL,
    .page_data = NULL,
    .attribute ={.type = LISAUI_PAGE_TYPE_PRIMARY_PAGE},
    .create = create,
    .destroy = destroy,
    .show = show,
    .close = close,
    .update_data = update_data,
};

LISAUI_PAGE_EXPORT(setting_main_page);
