#ifndef __MRPC_AP_PM_LOCK_API_MSG_H__
#define __MRPC_AP_PM_LOCK_API_MSG_H__

#include <stdint.h>
#include "mrpc.h"

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_ap_pm_lock_acquire_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_ap_pm_lock_acquire_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_ap_pm_lock_release_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_ap_pm_lock_release_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_ap_pm_lock_get_state_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    uint32_t locked;
} mrpc_ap_pm_lock_get_state_resp_t;

#endif /* __MRPC_AP_PM_LOCK_API_MSG_H__ */
