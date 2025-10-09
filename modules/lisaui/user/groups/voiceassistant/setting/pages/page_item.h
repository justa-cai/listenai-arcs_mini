#ifndef __SETTING_PAGE_ITEM_H__
#define __SETTING_PAGE_ITEM_H__

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SETTING_ITEM_WIFI_INDEX  (0)
#define SETTING_ITEM_COMMON_INDEX  (1)
#define SETTING_ITEM_MAX_NUMBER     (2)
struct setting_item {
    char *name;
    void *icon;
    lv_obj_t *scr;
    lv_obj_t *(*create)(lv_obj_t *parent,void* userdata);
};

extern struct setting_item *setting_items[SETTING_ITEM_MAX_NUMBER];

typedef struct{
    uint32_t item_index;
}page_item_data_t;

#ifdef __cplusplus
}
#endif

#endif /* __SETTING_PAGE_ITEM_H__ */