/**
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>

#include "lisaui_log.h"
#include "lisaui_type.h"
#include "assets/assets_res.h"
#include "lisaui_group.h"
#include "group_ocr.h"
#include "lisaui_manager.h"
#include "platform.h"
#include "user_groups.h"

static const char *TAG = "group.ocr";


static lisaui_err_t group_ocr_setup(lisaui_group_t *group)
{
    if((group == NULL)||(group->page_stack != NULL)){
        return LISAUI_ERR_APP_ID_INVALID;
    }

    group->page_stack = lisaui_page_stack_create();
    if(group->page_stack == NULL){
        return LISAUI_ERR_NO_MEMORY;
    }
   
    return LISAUI_ERR_OK;
}

static lisaui_err_t group_ocr_cleanup(lisaui_group_t *group)
{
    if(group == NULL){
        return LISAUI_ERR_APP_ID_INVALID;
    }
    
    if(group->page_stack != NULL){
        //遍历所有页面并销毁数据
        //销毁页面栈空间
    }
    return LISAUI_ERR_OK;
}

static lisaui_err_t group_ocr_enter(lisaui_group_t *group,lisaui_group_enter_page_method_t method,int page_index)
{
    lisaui_page_t *page;
    lisaui_page_t *new_page;
    lisaui_err_t ret = LISAUI_ERR_OK;
    
    LISAUI_LOGI(TAG, "Entering ocr group with method: %d, page_index: %d", method, page_index);

    if((group == NULL)||(group->page_stack == NULL)){
        LISAUI_LOGE(TAG, "Invalid group or page stack is NULL");
        return LISAUI_ERR_APP_ID_INVALID;
    }

    if(method == GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX){
        LISAUI_LOGI(TAG, "Using fixed page index method: %d", page_index);
        page = lisaui_page_stack_pop_by_index(group->page_stack,page_index);
        if(NULL == page){
            LISAUI_LOGI(TAG, "Page with index %d not found in stack, trying to create", page_index);
            if(page_index == LISAUI_GROUP_OCR_PAGE_INDEX_MAIN){
                LISAUI_LOGI(TAG, "Creating main page for ocr group");
                new_page = lisaui_page_create(LISAUI_GROUP_INDEX_OCR,LISAUI_GROUP_OCR_PAGE_INDEX_MAIN);
                if(new_page == NULL){
                    LISAUI_LOGE(TAG, "Failed to create main page");
                    ret = LISAUI_ERR_APP_PAGE_NOT_EXIST;
                    goto _ERR;
                }
                LISAUI_LOGI(TAG, "Main page created successfully");
                LISAUI_LOGI(TAG, "Showing newly created main page");
                new_page->show(new_page);
                group->current_page = new_page;
            }
            else {
                LISAUI_LOGE(TAG, "Requested page index %d is not available and cannot be created", page_index);
                ret = LISAUI_ERR_APP_PAGE_NOT_EXIST;
                goto _ERR;
            }
        }
    }
    else{
        LISAUI_LOGI(TAG, "Using stack top page method");
        page = lisaui_page_stack_pop(group->page_stack);
        if(NULL == page){
            page = lisaui_page_create(LISAUI_GROUP_INDEX_OCR,LISAUI_GROUP_OCR_PAGE_INDEX_MAIN);
            if(page == NULL){
                LISAUI_LOGE(TAG, "Failed to create main page");
                ret = LISAUI_ERR_APP_PAGE_NOT_EXIST;
                goto _ERR;
            }
        }
        LISAUI_LOGI(TAG, "Successfully popped page from stack, page index: %d", page->page_index);
        /* 标记当前页面并显示 */
        page->show(page);
        group->current_page = page;
        LISAUI_LOGI(TAG, "Page has been set as current and shown");
    }

    group->is_active = true;
    LISAUI_LOGI(TAG, "Successfully entered ocr group");
    return LISAUI_ERR_OK;

_ERR:
    LISAUI_LOGE(TAG, "Failed to enter ocr group, error code: %d", ret);
    return ret;   


}

static lisaui_err_t group_ocr_exit(lisaui_group_t *group)
{
    if(group->current_page != NULL){
        group->current_page->close(group->current_page);
        lisaui_page_stack_push(group->page_stack, group->current_page);
    }
    group->is_active = false;
    return LISAUI_ERR_OK;
}


static int manager_event_cb(uint32_t events,lisaui_group_t* sender,void* user_data){

    lisaui_group_t* _group = (lisaui_group_t*)user_data;
    lisaui_page_t *page;

    if(_group->is_active == false){
        return LISAUI_ERR_OK;
    }
    LISAUI_LOGI(TAG,"Lisaui manager events:0x%x,trigger by group:%s",events,sender->info.name);
    if(events & LISAUI_MANAGER_EVENT_SYS_BUTTON_BACK_CLICKED){
        if(_group->current_page != NULL){
            _group->current_page->close(_group->current_page);
            page = lisaui_page_stack_pop(_group->page_stack);
            if(NULL == page){
                lisaui_manager_group_exit(_group->info.id);
            }
            else{
                _group->current_page = page;
                _group->current_page->show(_group->current_page);
            }
        }
    }

}


static group_icon_t icon_res = {
    .hidden_icon = false,
    .title = "OCR",
    .res = &icon_img_app_default_png,
    .zoom = GROUP_ICON_ZOOM(0),
};

static lisaui_group_t group_ocr = {
    .setup = group_ocr_setup,
    .cleanup = group_ocr_cleanup,
    .enter = group_ocr_enter,
    .exit = group_ocr_exit,
    .info =
        {
            .name = "lisaui.ocr",
            .package_name = "com.listenai.lisaui.ocr",
            .id = LISAUI_GROUP_INDEX_OCR,
            .type = LISAUI_GROUP_TYPE_USER,
            .keep_in_stack = false,
        },
    .icon = &icon_res,
    .page_stack = NULL,
    .private_data = NULL,
};



static lisaui_err_t group_ocr_init(void)
{
    LISAUI_LOGI(TAG,"%s",__FUNCTION__);
    lisaui_manager_group_register(&group_ocr);
    lisaui_manager_add_callback(LISAUI_MANAGER_EVENT_SYS_ALL,
                                manager_event_cb,
                                &group_ocr);
    return LISAUI_ERR_OK;
}

LISAUI_GROUP_REGISTER(group_ocr,group_ocr_init,2);
