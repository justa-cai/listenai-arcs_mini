/**
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lisaui_group.h"
#include "lisaui_manager.h"
#include "lisaui_sys_events.h"
#include "lisa_ui_assets.h"
#include "lv_port_file.h"

void lisaui_ui_init(void)
{
    // if(lisaui_sys_events_init() != 0){
    //     return;
    // }
#if CONFIG_FILE_SYSTEM
    lv_port_fs_init();
#endif
    extern void lisa_ui_init(void);
    lisa_ui_init();
    lisaui_manager_init();
    lisaui_group_init();
}
