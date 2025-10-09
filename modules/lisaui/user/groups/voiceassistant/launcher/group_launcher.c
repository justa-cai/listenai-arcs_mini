/**
 *
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
#include "pages/launcher_pages.h"
#include "user_groups.h"
#include "group_launcher.h"

static const char *TAG = "group.launcher";

static lisaui_err_t group_launcher_setup(lisaui_group_t *group)
{
    return GROUP_BASE_SETUP_DEFAULT(group);
}

static lisaui_err_t group_launcher_cleanup(lisaui_group_t *group)
{
    return GROUP_BASE_CLEANUP_DEFAULT(group);
}

static lisaui_err_t group_launcher_enter(lisaui_group_t *group, lisaui_group_enter_page_method_t method, int page_index,
                                         uint32_t flags)
{
    return GROUP_BASE_ENTER_DEFAULT(group, method, page_index, flags);
}

static lisaui_err_t group_launcher_exit(lisaui_group_t *group)
{
    return GROUP_BASE_EXIT_DEFAULT(group);
}

static group_icon_t icon_res = {
    .hidden_icon = true,
    .title = "Launcher",
    .res = NULL,
    .zoom = GROUP_ICON_ZOOM(0),
};

static lisaui_group_t group_launcher = {
    .setup = group_launcher_setup,
    .cleanup = group_launcher_cleanup,
    .enter = group_launcher_enter,
    .exit = group_launcher_exit,
    .main_page_index = LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY,
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

static lisaui_err_t group_launcher_init(void)
{
    LISAUI_LOGI(TAG, "%s", __FUNCTION__);
    lisaui_manager_group_register(&group_launcher);
    lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                               LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY,0);
    return LISAUI_ERR_OK;
}

LISAUI_GROUP_REGISTER(group_launcher, group_launcher_init, 1);
