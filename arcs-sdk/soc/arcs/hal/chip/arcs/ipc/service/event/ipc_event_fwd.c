#include "string.h"
#include "ls_err.h"
#include "ls_list.h"
#include "log_print.h"
#include "rtos_al.h"
#include "ls_event.h"
#include "ipc_utils.h"

__attribute__((weak))  ls_err_t ls_event_post(event_module_t event_module_id, int event_id,
                        void *event_data, size_t event_data_size, uint32_t timeout, bool sync)
{
    struct cfg_ind_event sys_event;
    ls_err_t ret = LS_OK;

    if (event_data_size <= CFG_IND_EVENT_LEN_MAX)
    {
        sys_event.hdr.id  = IPC_IND_EVENT;
        sys_event.hdr.len = sizeof(struct cfg_ind_event) - sizeof(struct ipc_msg_hdr);
        sys_event.module_id  = event_module_id;
        sys_event.event_id   = event_id;
        memcpy(sys_event.event_data, event_data, event_data_size);
        sys_event.event_data_size = event_data_size;
        if (ipc_peer_msg_push(IPC_EP_IND, sizeof(struct cfg_ind_event), &sys_event))
            ret = LS_FAIL;
    }
    else
    {
        ret = LS_FAIL;
        CLOGE("Err: event data is too long\n");
    }

    return ret;
}
