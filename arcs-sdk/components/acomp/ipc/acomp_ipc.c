#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "assert.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "ic_message.h"
#include "acomp_ipc.h"
#include "gcl_cb_list/gcl_cb_list.h"
#include "dlist.h"
#include "acomp_err.h"

#define TAG "acomp_ipc"
#include "lisa_log.h"

#define CACHE_LINE_SIZE (32)
#define ASSERT(exp, fmt, ...)                                                                                          \
    do {                                                                                                               \
        if (!(exp)) {                                                                                                  \
            LISA_LOGI(TAG, "error:" fmt, ##__VA_ARGS__);                                                               \
            assert(exp);                                                                                               \
        }                                                                                                              \
    } while (0)
#define IPC_TIMOUT_MS (10000)

typedef struct {
    uint32_t dev_index;
    ipc_event_cb_t cb;
    void *priv;
    sys_dnode_t node;
} acomp_cb_list_item_t;

typedef struct {
    uint32_t index;
    uint8_t name[ACOMP_DEV_NAME_MAX_LEN];
    sys_dnode_t node;
} acomp_ipc_dev_info_list_item_t;

typedef struct {
    sys_dlist_t cb_list;
    sys_dlist_t dev_info_list;
    SemaphoreHandle_t reply_sem;

} acomp_ipc_handle_t;

acomp_ipc_handle_t *ipc_handle = NULL;

static int32_t acomp_ipc_callback_wrap(ic_message_handle_info_t *handle_info, ic_message_msg_info_t *msg)
{
    sys_dnode_t *node;
    acomp_cb_list_item_t *cb_item;
    acomp_ipc_handle_t *handle = (acomp_ipc_handle_t *)handle_info->user_datas;
    acomp_ipc_message_t *ipc_msg = (acomp_ipc_message_t *)msg->msg;

    if (ipc_msg->hdr.hdr.cmd == ACOMP_CONTEXT_IPC_GLB_REPLY) {
        xSemaphoreGive(handle->reply_sem);
        return 0;
    }

    if ((ipc_msg->address != 0) && (ipc_msg->len > 0)) {
        HAL_InvalidateDCache_by_Addr((void *)ipc_msg->address, ipc_msg->len);
    }

    if (ipc_msg->hdr.hdr.cmd == ACOMP_CONTEXT_IPC_GLB_NOTIFY) {
        SYS_DLIST_FOR_EACH_NODE(&handle->cb_list, node)
        {
            cb_item = CONTAINER_OF(node, acomp_cb_list_item_t, node);
            if (cb_item->dev_index == ipc_msg->dev_index) {
                cb_item->cb(ipc_msg, cb_item->priv);
            }
        }
    } else if (ipc_msg->hdr.hdr.cmd == ACOMP_CONTEXT_IPC_GLB_DEVINFO_QUERY_RESP) {
        acomp_ipc_dev_info_query_msg_t *dev_info = (acomp_ipc_dev_info_query_msg_t *)ipc_msg->address;
        for (int ii = 0; ii < dev_info->number; ii++) {
            acomp_ipc_dev_info_list_item_t *dev_info_item =
                (acomp_ipc_dev_info_list_item_t *)psram_malloc(sizeof(acomp_ipc_dev_info_list_item_t));
            if (dev_info_item == NULL) {
                LISA_LOGE(TAG, "acomp ipc dev info item malloc failed");
                return -1;
            }
            memset(dev_info_item, 0, sizeof(acomp_ipc_dev_info_list_item_t));
            dev_info_item->index = dev_info->item[ii].index;
            snprintf(dev_info_item->name, sizeof(dev_info_item->name), "%s", dev_info->item[ii].name);
            sys_dlist_append(&handle->dev_info_list, &dev_info_item->node);
            LISA_LOGI(TAG, "[%d]acomp remote dev index %d name %s", ii, dev_info_item->index, dev_info_item->name);
        }
    } else {
        LISA_LOGE(TAG, "acomp ipc unknown cmd:%d", ipc_msg->hdr.hdr.cmd);
    }

    if (ipc_msg->hdr.hdr.req_reply) {
        acomp_ipc_build_frame_send_sync(ipc_msg->dev_index, ACOMP_CONTEXT_IPC_GLB_REPLY, 0, 0, 0, 0);
    }

    return 0;
}

int acomp_ipc_init(void)
{
    int ret;

    if (ipc_handle != NULL) {
        return -ACOMP_ERR_INVALID_STATE;
    }

    ipc_handle = (acomp_ipc_handle_t *)psram_malloc(sizeof(acomp_ipc_handle_t));
    if (ipc_handle == NULL) {
        return -ACOMP_ERR_NO_MEM;
    }

    memset(ipc_handle, 0, sizeof(acomp_ipc_handle_t));
    sys_dlist_init(&ipc_handle->cb_list);
    sys_dlist_init(&ipc_handle->dev_info_list);
    ipc_handle->reply_sem = xSemaphoreCreateBinary();

    ic_message_register_by_id(IC_MESSAGE_ID_ACOMP, acomp_ipc_callback_wrap, ipc_handle);

    ret =
        acomp_ipc_build_frame_send_sync(0, ACOMP_CONTEXT_IPC_GLB_DEVINFO_QUERY | IPC_HEADER_REQ_REPALY, 0, 0, NULL, 0);

    return ret;
}

int acomp_ipc_add_callback(uint32_t dev_index, ipc_event_cb_t cb, void *priv)
{
    acomp_cb_list_item_t *cb_item;

    cb_item = (acomp_cb_list_item_t *)psram_malloc(sizeof(acomp_cb_list_item_t));
    if (cb_item == NULL) {
        return -ACOMP_ERR_NO_MEM;
    }
    memset(cb_item, 0, sizeof(acomp_cb_list_item_t));
    cb_item->dev_index = dev_index;
    cb_item->cb = cb;
    cb_item->priv = priv;
    sys_dlist_append(&ipc_handle->cb_list, &cb_item->node);

    return 0;
}

int acomp_ipc_remove_callback(uint32_t dev_index, ipc_event_cb_t cb)
{
    acomp_cb_list_item_t *cb_item;
    sys_dnode_t *node, *tmp;
    int ret = -ACOMP_ERR_NOT_FOUND;

    if (ipc_handle == NULL || cb == NULL) {
        return -EINVAL;
    }

    // Search for the callback item with matching dev_index
    SYS_DLIST_FOR_EACH_NODE_SAFE(&ipc_handle->cb_list, node, tmp)
    {
        cb_item = CONTAINER_OF(node, acomp_cb_list_item_t, node);
        if (cb_item->dev_index == dev_index) {
            sys_dlist_remove(&cb_item->node);
            psram_free(cb_item);
            return 0;
        }
    }

    return ret;
}

int acomp_ipc_build_frame_send_sync(int dev_index, int cmd, int acomp_cmd, uint8_t flags, void *data, uint16_t len)
{
    acomp_ipc_message_t ipc_msg;
    uint8_t *pdata;
    int ret = 0;

    ipc_msg.hdr.glb_cmd = cmd;
    ipc_msg.dev_index = dev_index;
    ipc_msg.acomp_cmd = acomp_cmd;
    ipc_msg.req.flags = flags;
    ipc_msg.len = len;
    ipc_msg.address = (uint32_t)data;

    if ((data != NULL) && (len > 0)) {
        ASSERT(!(((uint32_t)data % CACHE_LINE_SIZE) && (len % CACHE_LINE_SIZE)),
               "comp ipc buffer not aligned cache line(32)");
        HAL_FlushDCache_by_Addr((void *)data, len);
    }
    pdata = (uint8_t *)&ipc_msg;
    ic_message_msg_send_by_id(IC_MESSAGE_ID_ACOMP, IC_MESSAGE_MSG_TYPE_CMD, (uint8_t *)&ipc_msg,
                              sizeof(acomp_ipc_message_t));

    if ((ipc_msg.hdr.hdr.cmd != ACOMP_CONTEXT_IPC_GLB_REPLY) && (ipc_msg.hdr.hdr.req_reply)) {
        if (xSemaphoreTake(ipc_handle->reply_sem, pdMS_TO_TICKS(IPC_TIMOUT_MS)) != pdTRUE) {
            CLOGE("[%s %d]urpc_send_async_client hdr(0x%x) timeout(%d),ret %d !\n", __FUNCTION__, __LINE__,
                  ipc_msg.hdr.glb_cmd, IPC_TIMOUT_MS, ret);
            ret = -ACOMP_ERR_TIMEOUT;
        }
    }

    return ret;
}

int acomp_ipc_get_dev_index(const char *name)
{
    int ret;
    sys_dnode_t *node;

    SYS_DLIST_FOR_EACH_NODE(&ipc_handle->dev_info_list, node)
    {
        acomp_ipc_dev_info_list_item_t *dev_info_item = CONTAINER_OF(node, acomp_ipc_dev_info_list_item_t, node);
        if (strcmp(dev_info_item->name, name) == 0) {
            return dev_info_item->index;
        }
    }

    return -ACOMP_ERR_NOT_FOUND;
}