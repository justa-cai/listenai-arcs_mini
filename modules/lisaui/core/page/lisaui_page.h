/**
 * @file page_stack.h
 * @brief Page stack management module, using dlist to implement stack-based page management
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_PAGE_H__
#define __LISAUI_PAGE_H__

#include "lisaui_type.h"
#include "utils/dlist.h"
#include "port/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LISAUI_PAGE_EXPORT(page)                                                                                       \
    LISAUI_PAGE_SECTION const lisaui_page_t *const __lisaui_page_##page = &page


#define LISAUI_PAGE_USE(page)                                                                                           \
    extern lisaui_page_t *__lisaui_page_##page;                                                                \
    __attribute__((unused)) void *lisa_page_unused##page = (void *)&__lisaui_page_##page

typedef enum {
    LISAUI_PAGE_TYPE_PRIMARY_PAGE = 0,
    LISAUI_PAGE_TYPE_DATA_PAGE,
} lisaui_page_type_e;

#define LISAUI_PAGE_FLAG_NAV_LOCKED (1 << 0) /*Disable switch page*/

typedef struct {
    lisaui_page_type_e type;
} lisaui_page_attribute_t;

typedef struct lisaui_page{
    const char* cname;
    int group_index;
    int page_index;
    void *view;      //
    void *page_data; // 页面数据
    lisaui_page_attribute_t attribute;
    uint32_t flags;
    sys_dnode_t node;

    struct lisaui_page *(*create)(struct lisaui_page *page);
    lisaui_err_t (*destroy)(struct lisaui_page *page);
    lisaui_err_t (*show)(struct lisaui_page *page);
    lisaui_err_t (*close)(struct lisaui_page *page);
    lisaui_err_t (*update_data)(struct lisaui_page *page,void *data);
    

} lisaui_page_t;


lisaui_page_t *lisaui_page_create(uint32_t group_id,uint32_t page_id);
int lisaui_page_destroy(lisaui_page_t *page);

#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_PAGE_H__ */