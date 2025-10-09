/**
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>

#include "lisaui_log.h"
#include "lisaui_type.h"
#include "assets/assets_res.h"
#include "lisaui_group.h"
#include "lisaui_group_base.h"
#include "lisaui_manager.h"
#include "platform.h"

#define TAG "group.base"

static lisaui_err_t group_base_setup(lisaui_group_t *group)
{
    if ((group == NULL) || (group->page_stack != NULL)) {
        return LISAUI_ERR_GROUP_ID_INVALID;
    }

    group->page_stack = lisaui_page_stack_create();

    if (group->page_stack == NULL) {
        return LISAUI_ERR_NO_MEMORY;
    }
    LISAUI_LOGI(TAG, "Group setup %s", group->info.name);
    return LISAUI_ERR_OK;
}

static lisaui_err_t group_base_cleanup(lisaui_group_t *group)
{
    if (group == NULL) {
        return LISAUI_ERR_GROUP_ID_INVALID;
    }

    if (group->page_stack != NULL) {
        for (;;) {
            lisaui_page_t *page = lisaui_page_stack_pop(group->page_stack);
            if (page == NULL) {
                break;
            }
            lisaui_page_destroy(page);
        }
    }

    if (group->current_page != NULL) {
        group->current_page->close(group->current_page);
        lisaui_page_destroy(group->current_page);
        group->current_page = NULL;
    }
    LISAUI_LOGI(TAG, "Group cleanup %s", group->info.name);
    return LISAUI_ERR_OK;
}

static lisaui_err_t group_base_enter(lisaui_group_t *group, lisaui_group_enter_page_method_t method, int page_index,
                                     uint32_t flags)
{
    lisaui_page_t *page;
    lisaui_page_t *new_page;
    lisaui_page_t *current_page = NULL;
    lisaui_err_t ret = LISAUI_ERR_OK;

    LISAUI_LOGI(TAG, "Entering group %s with page config:method(%d), page_index(*%d)", group->info.name, method,
                page_index);

    if ((group == NULL) || (group->page_stack == NULL)) {
        LISAUI_LOGE(TAG, "Invalid group or page stack is NULL");
        return LISAUI_ERR_GROUP_ID_INVALID;
    }

    if (group->current_page != NULL) {
        current_page = group->current_page;
        if (flags & LISAUI_PAGE_SAVE_TO_HISTORY) {
            LISAUI_LOGI(TAG, "push page:%s to stack", group->current_page->cname);
            lisaui_page_stack_push(group->page_stack, group->current_page);
            group->current_page = NULL;
        } else {
            group->current_page = NULL;
        }
    }

    if (method == GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX) {
        LISAUI_LOGI(TAG, "Using fixed page index method: %d", page_index);
        page = lisaui_page_stack_pop_by_index(group->page_stack, page_index);
        if (NULL == page) {
            LISAUI_LOGI(TAG, "Page with index %d not found in stack, trying to create", page_index);
            new_page = lisaui_page_create(group->info.id, page_index);
            if (new_page == NULL) {
                LISAUI_LOGE(TAG, "Failed to create page with index %d", page_index);
                ret = LISAUI_ERR_PAGE_NOT_EXIST;
                goto _ERR;
            }
            LISAUI_LOGI(TAG, "Page %s created successfully", new_page->cname);
            new_page->show(new_page);
            group->current_page = new_page;
        } else {
            LISAUI_LOGI(TAG, "Page %s found in stack, show it", page->cname);
            page->show(page);
            group->current_page = page;
        }
    } else {
        LISAUI_LOGI(TAG, "Using stack top page method");
        page = lisaui_page_stack_pop(group->page_stack);
        if (NULL == page) {
            page = lisaui_page_create(group->info.id, group->main_page_index);
            if (page == NULL) {
                LISAUI_LOGE(TAG, "Failed to create default page with index %d", group->main_page_index);
                ret = LISAUI_ERR_PAGE_NOT_EXIST;
                goto _ERR;
            }
        }
        LISAUI_LOGI(TAG, "Successfully popped page from stack, page index: %d", page->page_index);
        /* 标记当前页面并显示 */
        page->show(page);
        group->current_page = page;
        LISAUI_LOGI(TAG, "Page has been set as current and shown");
    }

    /*clear page stack*/
    if(group->current_page->attribute.type == LISAUI_PAGE_TYPE_PRIMARY_PAGE){
        for (;;) {
            page = lisaui_page_stack_pop(group->page_stack);
            if (page == NULL) {
                break;
            }
            lisaui_page_destroy(page);
        }
    }

    /**/
    if (current_page!= NULL) {
        current_page->close(current_page);
        if (!(flags & LISAUI_PAGE_SAVE_TO_HISTORY)) {
            lisaui_page_destroy(current_page);
        }
    }
    group->is_active = true;
    LISAUI_LOGI(TAG, "Group enter %s", group->info.name);
    return LISAUI_ERR_OK;

_ERR:
    LISAUI_LOGE(TAG, "Failed to enter group, error code: %d", ret);
    return ret;
}

static lisaui_err_t group_base_exit(lisaui_group_t *group)
{
    lisaui_page_t *page = NULL;
    if (group == NULL) {
        return LISAUI_ERR_GROUP_ID_INVALID;
    }

    for (;;) {
        page = lisaui_page_stack_pop(group->page_stack);
        if (page == NULL) {
            break;
        }
        lisaui_page_destroy(page);
    }

    if (group->current_page != NULL) {
        group->current_page->close(group->current_page);
        lisaui_page_destroy(group->current_page);
        group->current_page = NULL;
    }
    group->is_active = false;
    LISAUI_LOGI(TAG, "Group exit %s", group->info.name);
    return LISAUI_ERR_OK;
}

static const lisaui_group_t group_base = {
    .setup = group_base_setup,
    .cleanup = group_base_cleanup,
    .enter = group_base_enter,
    .exit = group_base_exit,
};

lisaui_group_t *group_base_get(void)
{
    return (lisaui_group_t *)&group_base;
}
