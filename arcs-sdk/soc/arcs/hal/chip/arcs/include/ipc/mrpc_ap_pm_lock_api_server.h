#ifndef __MRPC_AP_PM_LOCK_API_SERVER_H__
#define __MRPC_AP_PM_LOCK_API_SERVER_H__

#include <stdint.h>
#include "mrpc_types.h"

typedef enum
{
    MRPC_MSG_ID_AP_PM_LOCK_START = MRPC_SERVICE_TYPE_AP_PM_LOCK << 24,
    MRPC_MSG_ID_AP_PM_LOCK_ACQUIRE,
    MRPC_MSG_ID_AP_PM_LOCK_RELEASE,
    MRPC_MSG_ID_AP_PM_LOCK_GET_STATE,
    MRPC_MSG_ID_AP_PM_LOCK_MAX
} ipc_msg_id_ap_pm_lock_t;

extern mrpc_msg_handler_t mrpc_msg_ap_pm_lock_handlers[
    MRPC_MSG_ID_AP_PM_LOCK_MAX - MRPC_MSG_ID_AP_PM_LOCK_START];

int32_t ap_pm_remote_lock_server_acquire(void);
int32_t ap_pm_remote_lock_server_release(void);
int32_t ap_pm_remote_lock_server_get_state(uint32_t *locked);

#endif /* __MRPC_AP_PM_LOCK_API_SERVER_H__ */
