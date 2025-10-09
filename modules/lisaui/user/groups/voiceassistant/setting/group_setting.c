/**
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>

#include "lisaui_log.h"
#include "lisaui_type.h"
#include "assets/assets_res.h"
#include "lisaui_group.h"
#include "lisaui_group_base.h"
#include "group_setting.h"
#include "lisaui_manager.h"
#include "platform.h"
#include "user_groups.h"

static const char *TAG = "group.setting";


static lisaui_err_t group_setting_setup(lisaui_group_t *group)
{
    return GROUP_BASE_SETUP_DEFAULT(group);
}

static lisaui_err_t group_setting_cleanup(lisaui_group_t *group)
{
    return GROUP_BASE_CLEANUP_DEFAULT(group);
}

static lisaui_err_t group_setting_enter(lisaui_group_t *group,lisaui_group_enter_page_method_t method,int page_index,uint32_t flags)
{
    return GROUP_BASE_ENTER_DEFAULT(group,method,page_index,flags);
}

static lisaui_err_t group_setting_exit(lisaui_group_t *group)
{
    return GROUP_BASE_EXIT_DEFAULT(group);
}

static group_icon_t icon_res = {
    .hidden_icon = true,
    .title = "setting",
    .res = &icon_img_app_default_png,
    .zoom = GROUP_ICON_ZOOM(0),
};

static lisaui_group_t group_setting = {
    .setup = group_setting_setup,
    .cleanup = group_setting_cleanup,
    .enter = group_setting_enter,
    .exit = group_setting_exit,
    .main_page_index = LISAUI_GROUP_SETTING_PAGE_INDEX_PRIMARY,
    .info =
        {
            .name = "lisaui.setting",
            .package_name = "com.listenai.lisaui.setting",
            .id = LISAUI_GROUP_INDEX_SETTING,
            .type = LISAUI_GROUP_TYPE_USER,
            .keep_in_stack = false,
        },
    .icon = &icon_res,
    .page_stack = NULL,
    .private_data = NULL,
};



static lisaui_err_t group_setting_init(void)
{
    LISAUI_LOGI(TAG,"%s",__FUNCTION__);
    lisaui_manager_group_register(&group_setting);

    return LISAUI_ERR_OK;
}

LISAUI_GROUP_REGISTER(group_setting,group_setting_init,2);
