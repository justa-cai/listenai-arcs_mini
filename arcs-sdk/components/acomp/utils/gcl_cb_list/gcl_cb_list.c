
#include "sysheap.h"
#include "gcl_cb_list.h"
#include "gcl_common.h"
#include "dlist.h"
#include <stddef.h>

struct gcl_cb {
    sys_dlist_t cb_list;
    gcl_mutex_t mutex;
};

typedef struct {
    sys_dnode_t node;
    gcl_event_cb_t cb;
    uint32_t events;
    void *arg;
} gcl_cb_list_item_t;

static inline gcl_cb_list_item_t *item_create(uint32_t events, gcl_event_cb_t cb, void *arg)
{
    gcl_cb_list_item_t *new_item = psram_malloc(sizeof(gcl_cb_list_item_t));
    if (!new_item) {
        return NULL;
    }
    new_item->cb = cb;
    new_item->arg = arg;
    new_item->events = events;
    return new_item;
}

static inline void item_delete(gcl_cb_list_item_t *item)
{
    gcl_heap_free(item);
}

gcl_cb_list_t gcl_cb_list_create(void)
{
    struct gcl_cb *gcl_cb = psram_malloc(sizeof(struct gcl_cb));
    if (gcl_cb == NULL) {
        return NULL;
    }
    sys_dlist_init(&gcl_cb->cb_list);
    gcl_mutex_init(&gcl_cb->mutex);
    return (gcl_cb_list_t)gcl_cb;
}

int gcl_cb_event_dispatch(gcl_cb_list_t gcl_cb_list, uint32_t event, void *event_data, uint32_t event_data_len)
{
    int ret = -GCL_ERR_NOT_FOUND;
    struct gcl_cb *gcl_cb = gcl_cb_list;
    sys_dlist_t *list = &gcl_cb->cb_list;
    sys_dnode_t *item, *tmp;
    gcl_cb_list_item_t *cb_item;

    gcl_mutex_lock(&gcl_cb->mutex);
    SYS_DLIST_FOR_EACH_NODE_SAFE(list, item, tmp) {
        cb_item = SYS_DLIST_CONTAINER(item, cb_item, node);
        if (cb_item->events & event) {
            cb_item->cb(cb_item->events & event, event_data, event_data_len, cb_item->arg);
            ret = 0;
        }
    }
    gcl_mutex_unlock(&gcl_cb->mutex);
    return ret;
}

int gcl_cb_list_add_callback(gcl_cb_list_t gcl_cb_list, uint32_t events, gcl_event_cb_t callback, void *arg)
{
    struct gcl_cb *gcl_cb = gcl_cb_list;

    gcl_cb_list_item_t *new_cb = item_create(events, callback, arg);
    if (!new_cb) {
        return -GCL_ERR_NO_MEM;
    }
    gcl_mutex_lock(&gcl_cb->mutex);
    sys_dlist_prepend(&gcl_cb->cb_list, &new_cb->node);
    gcl_mutex_unlock(&gcl_cb->mutex);
    return GCL_OK;
}

int gcl_cb_list_remove_callback(gcl_cb_list_t gcl_cb_list, gcl_event_cb_t callback)
{
    int ret = -GCL_ERR_NOT_FOUND;
    struct gcl_cb *gcl_cb = gcl_cb_list;
    sys_dlist_t *list = &gcl_cb->cb_list;
    sys_dnode_t *item, *tmp;
    gcl_cb_list_item_t *cb_item;

    gcl_mutex_lock(&gcl_cb->mutex);
    SYS_DLIST_FOR_EACH_NODE_SAFE(list, item, tmp) {
        cb_item = SYS_DLIST_CONTAINER(item, cb_item, node);
        if (cb_item->cb == callback) {
            sys_dlist_remove(&cb_item->node);
            item_delete(cb_item);
            ret = 0;
            break;
        }
    }
    gcl_mutex_unlock(&gcl_cb->mutex);
    return ret;
}

void gcl_cb_list_clean_callbacks(gcl_cb_list_t gcl_cb_list)
{
    struct gcl_cb *gcl_cb = gcl_cb_list;
    sys_dlist_t *list = &gcl_cb->cb_list;
    sys_dnode_t *item, *tmp;
    gcl_cb_list_item_t *cb_item;

    gcl_mutex_lock(&gcl_cb->mutex);
    SYS_DLIST_FOR_EACH_NODE_SAFE(list, item, tmp) {
        cb_item = SYS_DLIST_CONTAINER(item, cb_item, node);
        sys_dlist_remove(&cb_item->node);
        item_delete(cb_item);
    }
    gcl_mutex_unlock(&gcl_cb->mutex);
}

void gcl_cb_list_delete(gcl_cb_list_t gcl_cb_list)
{
    struct gcl_cb *gcl_cb = gcl_cb_list;
    gcl_cb_list_clean_callbacks(gcl_cb_list);
    gcl_mutex_delete(&gcl_cb->mutex);
    gcl_heap_free(gcl_cb_list);
}
