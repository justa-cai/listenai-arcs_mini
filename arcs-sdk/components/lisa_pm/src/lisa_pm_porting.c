#include <stddef.h>

#include "lisa_pm_porting.h"

extern const lisa_pm_porting_ops_t g_lisa_pm_porting_ops;

static inline const lisa_pm_porting_ops_t *lisa_pm_porting_get_ops(void)
{
    return &g_lisa_pm_porting_ops;
}

#define CALL_PORTING_OP(op, ...) lisa_pm_porting_get_ops()->op(__VA_ARGS__)

int32_t lisa_pm_porting_init(void)
{
    return CALL_PORTING_OP(init);
}

int32_t lisa_pm_porting_apply_policy(lisa_pm_system_policy_t policy)
{
    return CALL_PORTING_OP(apply_policy, policy);
}

int32_t lisa_pm_porting_acquire_lock(void)
{
    return CALL_PORTING_OP(acquire_lock);
}

int32_t lisa_pm_porting_release_lock(void)
{
    return CALL_PORTING_OP(release_lock);
}

int32_t lisa_pm_porting_register_sleep_hooks(void)
{
    return CALL_PORTING_OP(register_sleep_hooks);
}

int32_t lisa_pm_porting_unregister_sleep_hooks(void)
{
    return CALL_PORTING_OP(unregister_sleep_hooks);
}

int32_t lisa_pm_porting_register_managed_device(void)
{
    return CALL_PORTING_OP(register_managed_device);
}

int32_t lisa_pm_porting_unregister_managed_device(void)
{
    return CALL_PORTING_OP(unregister_managed_device);
}

lisa_pm_wakeup_cause_t lisa_pm_porting_get_wakeup_cause(void)
{
    return CALL_PORTING_OP(get_wakeup_cause);
}

lisa_pm_wakeup_cause_t lisa_pm_porting_map_wakeup_cause(uint32_t cause)
{
    return CALL_PORTING_OP(map_wakeup_cause, cause);
}

#if CONFIG_LISA_PM_REMOTE_LOCK_CLIENT
int32_t lisa_pm_porting_remote_lock_acquire(void)
{
    if (lisa_pm_porting_get_ops()->remote_lock_acquire == NULL) {
        return -1;
    }

    return CALL_PORTING_OP(remote_lock_acquire);
}

int32_t lisa_pm_porting_remote_lock_release(void)
{
    if (lisa_pm_porting_get_ops()->remote_lock_release == NULL) {
        return -1;
    }

    return CALL_PORTING_OP(remote_lock_release);
}

int32_t lisa_pm_porting_remote_lock_get_state(lisa_pm_remote_lock_state_t *state)
{
    if (lisa_pm_porting_get_ops()->remote_lock_get_state == NULL) {
        return -1;
    }

    return CALL_PORTING_OP(remote_lock_get_state, state);
}
#endif

#undef CALL_PORTING_OP
