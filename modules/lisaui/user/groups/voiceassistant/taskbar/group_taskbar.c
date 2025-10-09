/**
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>

#include "lisaui_log.h"
#include "lisaui_type.h"
#include "assets/assets_res.h"
#include "lisaui_group.h"
#include "lisaui_group_base.h"
#include "group_taskbar.h"
#include "lisaui_manager.h"
#include "platform.h"
#include "user_groups.h"

static const char *TAG = "group.taskbar";

static lisaui_err_t group_taskbar_setup(lisaui_group_t *group)
{
    int ret;
    lisaui_page_t *new_page;

    ret = GROUP_BASE_SETUP_DEFAULT(group);
    if (ret != LISAUI_ERR_OK) {
        return ret;
    }

    new_page = lisaui_page_create(LISAUI_GROUP_INDEX_TASKBAR, LISAUI_GROUP_TASKBAR_PAGE_INDEX_NAV);
    if (new_page == NULL) {
        LISAUI_LOGE(TAG, "Failed to create nav page");
        ret = LISAUI_ERR_PAGE_NOT_EXIST;
        return ret;
    }
    LISAUI_LOGI(TAG, "Nav page created successfully");
    new_page->show(new_page);
    group->current_page = new_page;
    return LISAUI_ERR_OK;
}

static lisaui_err_t group_taskbar_cleanup(lisaui_group_t *group)
{
    return GROUP_BASE_CLEANUP_DEFAULT(group);
}

static lisaui_err_t group_taskbar_enter(lisaui_group_t *group, lisaui_group_enter_page_method_t method, int page_index,
                                        uint32_t flags)
{
    return GROUP_BASE_ENTER_DEFAULT(group, method, page_index, flags);
}

static lisaui_err_t group_taskbar_exit(lisaui_group_t *group)
{
    return GROUP_BASE_EXIT_DEFAULT(group);
}

static group_icon_t icon_res = {
    .hidden_icon = true,
    .title = "TASKBAR",
    .res = &icon_img_app_default_png,
    .zoom = GROUP_ICON_ZOOM(0),
};

static lisaui_group_t group_taskbar = {
    .setup = group_taskbar_setup,
    .cleanup = group_taskbar_cleanup,
    .enter = group_taskbar_enter,
    .exit = group_taskbar_exit,
    .main_page_index = LISAUI_GROUP_TASKBAR_PAGE_INDEX_NAV,
    .info =
        {
            .name = "lisaui.taskbar",
            .package_name = "com.listenai.lisaui.taskbar",
            .id = LISAUI_GROUP_INDEX_TASKBAR,
            .type = LISAUI_GROUP_TYPE_USER,
            .keep_in_stack = false,
        },
    .icon = &icon_res,
    .page_stack = NULL,
    .private_data = NULL,
};

static lisaui_err_t group_taskbar_init(void)
{
    LISAUI_LOGI(TAG, "%s", __FUNCTION__);
    lisaui_manager_group_register(&group_taskbar);

    return LISAUI_ERR_OK;
}

LISAUI_GROUP_REGISTER(group_taskbar, group_taskbar_init, 2);
