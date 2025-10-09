/**
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_GROUP_H__
#define __LISAUI_GROUP_H__

#include "dlist.h"
#include "lisaui_type.h"
#include "lisaui_stack_page.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LISAUI_GROUP_INDEX_RESERVED_ALL  (0) /* 保留ID,表述所有注册的组 */
#define LISAUI_GROUP_INDEX_RESERVED_NUM  (5) /* 保留ID数量*/


#ifndef GROUP_ICON_ZOOM
#define GROUP_ICON_ZOOM(x) ((uint16_t)((x) * 256))
#endif

typedef struct {
    bool hidden_icon;
    const char *title;
    lisaui_icon_res_t *res;
    lv_obj_t *lv_obj_icon;
    uint16_t zoom;
    uint16_t icon_width;
    uint16_t icon_height;
}group_icon_t;

typedef enum {
    LISAUI_GROUP_TYPE_NORMAL = 0,
    LISAUI_GROUP_TYPE_USER,
    LISAUI_GROUP_TYPE_SYSTEM,
    LISAUI_GROUP_TYPE_LAUNCHER
} lisaui_group_type_e;

struct group_info_t {
    const char *name;
    const char *package_name;
    const int id;
    int uuid;
    const lisaui_group_type_e type;
    bool keep_in_stack;
};
typedef enum {
    GROUP_ENTER_PAGE_METHOD_POP_STACK_TOP = 0,
    GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
} lisaui_group_enter_page_method_t;


typedef struct lisaui_group {
    sys_dnode_t node;
    lisaui_err_t (*setup)(struct lisaui_group *group);
    lisaui_err_t (*cleanup)(struct lisaui_group *group);
    lisaui_err_t (*enter)(struct lisaui_group *group, lisaui_group_enter_page_method_t method, int page_index,uint32_t flags);
    lisaui_err_t (*exit)(struct lisaui_group *group);

    struct group_info_t info;
    bool is_active;
    group_icon_t *icon;
    int main_page_index;
    lisaui_page_stack_t *page_stack;
    lisaui_page_t *current_page;
    void *private_data;
} lisaui_group_t;

typedef struct {
    lisaui_group_t *group;
    lisaui_err_t (*init_func)(void);
} group_entry_t;


#define LISAUI_GROUP_REGISTER(gp, func, prio)  \
    LISAUI_GROUP_SECTION_LEVEL##prio const group_entry_t lisaui_group_##gp##_entry = { \
        .group = &gp,                                                           \
        .init_func = func,                                                                                             \
    }

#define LISAUI_USE_GROUP(name)                                                                                           \
    extern const group_entry_t lisaui_group_##name##_entry;                                                                \
    __attribute__((unused)) void *_app_##name = (void *)&lisaui_group_##name##_entry


/**
 * @brief Find a group by its ID
 * 
 * This function searches for a group with the specified ID.
 *
 * @param index The ID of the group to find
 * @return lisaui_group_t* Pointer to the group if found, NULL otherwise
 */
lisaui_group_t *lisaui_group_find_by_id(int index);

extern int lisaui_group_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_GROUP_H__ */