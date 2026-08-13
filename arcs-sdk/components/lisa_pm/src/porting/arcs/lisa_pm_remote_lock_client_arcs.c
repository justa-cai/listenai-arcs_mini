/**
 * @file lisa_pm_remote_lock_client_arcs.c
 * @brief ARCS MRPC implementation for CP-side LISA PM remote lock.
 */

#include <stddef.h>

#include "lisa_pm.h"
#include "mrpc_ap_pm_lock_api_client.h"

int32_t lisa_pm_porting_arcs_remote_lock_acquire(void)
{
    return ap_pm_remote_lock_acquire();
}

int32_t lisa_pm_porting_arcs_remote_lock_release(void)
{
    return ap_pm_remote_lock_release();
}

int32_t lisa_pm_porting_arcs_remote_lock_get_state(lisa_pm_remote_lock_state_t *state)
{
    if (state == NULL) {
        return -1;
    }

    return ap_pm_remote_lock_get_state(&state->locked);
}
