#ifndef __MRPC_AP_PM_LOCK_API_CLIENT_H__
#define __MRPC_AP_PM_LOCK_API_CLIENT_H__

#include <stdint.h>

int32_t ap_pm_remote_lock_acquire(void);
int32_t ap_pm_remote_lock_release(void);
int32_t ap_pm_remote_lock_get_state(uint32_t *locked);

#endif /* __MRPC_AP_PM_LOCK_API_CLIENT_H__ */
