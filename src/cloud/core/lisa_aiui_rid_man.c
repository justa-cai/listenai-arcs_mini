#include "stdint.h"
#include "lisa_mutex.h"
#include "stddef.h"
#include "string.h"

struct lisa_aiui_rid_t {
    uint8_t used;
    uint32_t rid;
    void *data;
};

static struct lisa_aiui_rid_t g_rid_list[10];
static lisa_mutex_t *g_rid_mutex = NULL;

#define LISA_UI_RID_LIST_SIZE() (sizeof(g_rid_list) / sizeof(g_rid_list[0]))

int lisa_aiui_rid_man_init(void)
{
    memset(g_rid_list, 0, sizeof(g_rid_list));
    g_rid_mutex = lisa_mutex_create();
    if (g_rid_mutex == NULL) {
        return -1;
    }

    return 0;
}

int lisa_ui_rid_list_clear(void)
{
    lisa_mutex_lock(g_rid_mutex, LISA_OS_WAIT_FOREVER);
    memset(g_rid_list, 0, sizeof(g_rid_list));
    lisa_mutex_unlock(g_rid_mutex);

    return 0;
}

int lisa_aiui_rid_list_add(uint32_t rid, void *data)
{
    lisa_mutex_lock(g_rid_mutex, LISA_OS_WAIT_FOREVER);

    for (int i = 0; i < LISA_UI_RID_LIST_SIZE(); i++) {
        if (g_rid_list[i].used == 0) {
            g_rid_list[i].rid = rid;
            g_rid_list[i].data = data;
            g_rid_list[i].used = 1;
            lisa_mutex_unlock(g_rid_mutex);
            return 0;
        }
    }
    lisa_mutex_unlock(g_rid_mutex);

    return -1;
}

int lisa_aiui_rid_list_remove(uint32_t rid)
{
    lisa_mutex_lock(g_rid_mutex, LISA_OS_WAIT_FOREVER);
    for (int i = 0; i < LISA_UI_RID_LIST_SIZE(); i++) {
        if (g_rid_list[i].used == 1 && g_rid_list[i].rid == rid) {
            g_rid_list[i].used = 0;
            lisa_mutex_unlock(g_rid_mutex);
            return 0;
        }
    }
    lisa_mutex_unlock(g_rid_mutex);

    return -1;
}

int lisa_aiui_rid_list_get(uint32_t rid, void **data)
{
    lisa_mutex_lock(g_rid_mutex, LISA_OS_WAIT_FOREVER);
    for (int i = 0; i < LISA_UI_RID_LIST_SIZE(); i++) {
        if (g_rid_list[i].used == 1 && g_rid_list[i].rid == rid) {
            *data = g_rid_list[i].data;
            lisa_mutex_unlock(g_rid_mutex);
            return 0;
        }
    }
    lisa_mutex_unlock(g_rid_mutex);

    return -1;
}
