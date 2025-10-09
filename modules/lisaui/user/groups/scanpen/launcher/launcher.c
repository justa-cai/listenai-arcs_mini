/**
 * @file app_launcher.c
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief
 * @version 0.1
 * @date 2025-02-25
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>

#include "lisaui_log.h"
#include "lisaui_type.h"
#include "assets/assets_res.h"
#include "lisaui_group.h"
#include "launcher.h"
#include "lisaui_manager.h"
#include "platform.h"
#include "pages/launcher_pages.h"
#include "user_groups.h"

static const char *TAG = "group.launcher";


lisaui_err_t group_launcher_setup(lisaui_group_t *group)
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

lisaui_err_t group_launcher_cleanup(lisaui_group_t *group)
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

/**
 * @brief Enter the launcher group with specified method and page index
 * 
 * This function is called when the system wants to display the launcher group.
 * It handles different entry methods:
 * - When GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX is used, it tries to find or create
 *   the requested page by index
 * - When GROUP_ENTER_PAGE_METHOD_POP_STACK_TOP is used, it displays the page at the
 *   top of the page stack
 *
 * If the requested page doesn't exist, it will create it. For the main page, it will
 * use the page creation function. For other pages, it returns an error.
 *
 * @param group Pointer to the launcher group structure
 * @param method Method to use when entering the group (stack top or specific page)
 * @param page_index Index of the page to show (used when method is GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX)
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t group_launcher_enter(lisaui_group_t *group, lisaui_group_enter_page_method_t method, int page_index,int is_push_stack)
{
    lisaui_page_t *page;
    lisaui_page_t *new_page;
    lisaui_err_t ret = LISAUI_ERR_OK;
    
    LISAUI_LOGI(TAG, "Entering launcher group with method: %d, page_index: %d", method, page_index);

    if((group == NULL)||(group->page_stack == NULL)){
        LISAUI_LOGE(TAG, "Invalid group or page stack is NULL");
        return LISAUI_ERR_APP_ID_INVALID;
    }

    if(method == GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX){
        LISAUI_LOGI(TAG, "Using fixed page index method: %d", page_index);
        page = lisaui_page_stack_pop_by_index(group->page_stack,page_index);
        if(NULL == page){

            LISAUI_LOGI(TAG, "Page with index %d not found in stack, trying to create", page_index);
            new_page = lisaui_page_create(LISAUI_GROUP_INDEX_LAUNCHER,page_index);
            if(new_page == NULL){
                LISAUI_LOGE(TAG, "Failed to create index:%d page",page_index);
                ret = LISAUI_ERR_APP_PAGE_NOT_EXIST;
                goto _ERR;
            }
            LISAUI_LOGI(TAG, "Page %s created and show successfully",new_page->cname);

            new_page->show(new_page);
            group->current_page = new_page;

        }
        else{
            LISAUI_LOGI(TAG, "Page %s found in stack,show it", page->cname);
            page->show(page);
            group->current_page = page;
        }
    }
    else{
        LISAUI_LOGI(TAG, "Using stack top page method");
        page = lisaui_page_stack_pop(group->page_stack);
        if(NULL == page){
            page = lisaui_page_create(LISAUI_GROUP_INDEX_LAUNCHER,LISAUI_GROUP_PAGE_INDEX_MAIN);
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
    LISAUI_LOGI(TAG, "Successfully entered launcher group");
    return LISAUI_ERR_OK;

_ERR:
    LISAUI_LOGE(TAG, "Failed to enter launcher group, error code: %d", ret);
    return ret;
}

lisaui_err_t group_launcher_exit(lisaui_group_t *group)
{
    if(group->current_page != NULL){
        group->current_page->close(group->current_page);
        lisaui_page_stack_push(group->page_stack, group->current_page);
        LISAUI_LOGI(TAG, "===============push page index:%d to stack",group->current_page->page_index);
    }
    group->is_active = false;
    return LISAUI_ERR_OK;
}


static group_icon_t icon_res = {
    .hidden_icon = true,
    .title = "Launcher",
    .res = &ui_img_icon_launcher_png,
    .zoom = GROUP_ICON_ZOOM(0),
};

static lisaui_group_t group_launcher = {
    .setup = group_launcher_setup,
    .cleanup = group_launcher_cleanup,
    .enter = group_launcher_enter,
    .exit = group_launcher_exit,
    .main_page_index = LISAUI_GROUP_LUNCHER_PAGE_INDEX_MAIN,
    .info =
        {
            .name = "launcher",
            .package_name = "com.listenai.lisaui.launcher",
            .id = LISAUI_GROUP_INDEX_LAUNCHER,
            .type = LISAUI_GROUP_TYPE_LAUNCHER,
            .keep_in_stack = true,
        },
    .icon = &icon_res,
    
    .page_stack = NULL,
    .private_data = NULL,
};

static int manager_event_cb(uint32_t events,lisaui_group_t* sender,void* user_data){

    lisaui_group_t* _group = (lisaui_group_t*)user_data;
    LISAUI_LOGI(TAG,"Lisaui manager events:0x%x,trigger by group:%s",events,sender->info.name);
    if(events & LISAUI_MANAGER_EVENT_GROUP_REGISTER){
        LISAUI_LOGI(TAG,"[%s %d]=======================",__FUNCTION__,__LINE__);
        lisaui_page_t *page;
        if((_group != NULL) 
            && (_group->current_page != NULL) 
            && (_group->current_page->page_index == LISAUI_GROUP_PAGE_INDEX_MAIN)){
            page = _group->current_page;
        }
        else{
            LISAUI_LOGI(TAG,"[%s %d]=======================",__FUNCTION__,__LINE__);    
            page = lisaui_page_stack_pop_by_index(sender->page_stack,LISAUI_GROUP_PAGE_INDEX_MAIN);
        }
        
        if(page != NULL){
            LISAUI_LOGI(TAG,"[%s %d]=======================",__FUNCTION__,__LINE__);
            launcher_page_update_parameter_t data = {
                .event = LAUNCHER_PAGE_MAIN_UPDATE_ADD_ICON,
                .update_icon = {
                    .group = sender,
                }
            };
            LISAUI_LOGI(TAG,"[%s %d]=======================",__FUNCTION__,__LINE__);
            page->update_data(page,&data);
        }
    }
    else if((events & LISAUI_MANAGER_EVENT_SYS_BUTTON_HOME_CLICKED)
            ||(events & LISAUI_MANAGER_EVENT_SYS_BUTTON_CLOSE_CLICKED)){

        lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER,GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,LISAUI_GROUP_PAGE_INDEX_MAIN);
    }   
    return LISAUI_ERR_OK;
}

static lisaui_err_t group_launcher_init(void)
{
    LISAUI_LOGI(TAG,"%s",__FUNCTION__);
    lisaui_manager_group_register(&group_launcher);
    lisaui_manager_add_callback(LISAUI_MANAGER_EVENT_GROUP_REGISTER | 
                                LISAUI_MANAGER_EVENT_GROUP_UNREGISTER|
                                LISAUI_MANAGER_EVENT_SYS_ALL,
                                manager_event_cb,
                                &group_launcher);
    lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER,GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,LISAUI_GROUP_PAGE_INDEX_MAIN);
    return LISAUI_ERR_OK;
}

LISAUI_GROUP_REGISTER(group_launcher, group_launcher_init,1);
