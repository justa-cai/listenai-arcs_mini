/**
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_GROUP_BASE_H__
#define __LISAUI_GROUP_BASE_H__

#include <stdbool.h>
#include "lisaui_type.h"
#include "lisaui_group.h"

#ifdef __cplusplus
extern "C" {
#endif


#define GROUP_BASE_SETUP_DEFAULT(group)  group_base_get()->setup(group)
#define GROUP_BASE_CLEANUP_DEFAULT(group) group_base_get()->cleanup(group)
#define GROUP_BASE_ENTER_DEFAULT(group, method, page_index, flags) group_base_get()->enter(group, method, page_index, flags)
#define GROUP_BASE_EXIT_DEFAULT(group) group_base_get()->exit(group)


lisaui_group_t *group_base_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_GROUP_BASE_H__ */