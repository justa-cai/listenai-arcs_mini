/**
 * @file lisa_pm_remote_lock.c
 * @brief LISA PM remote lock public wrappers.
 */

#include <stddef.h>

#include "lisa_pm.h"
#include "lisa_pm_porting.h"

#if CONFIG_LISA_PM_REMOTE_LOCK_CLIENT
int32_t lisa_pm_remote_lock_acquire(void)
{
    return lisa_pm_porting_remote_lock_acquire();
}

int32_t lisa_pm_remote_lock_release(void)
{
    return lisa_pm_porting_remote_lock_release();
}

int32_t lisa_pm_remote_lock_get_state(lisa_pm_remote_lock_state_t *state)
{
    if (state == NULL) {
        return -1;
    }

    return lisa_pm_porting_remote_lock_get_state(state);
}
#endif
