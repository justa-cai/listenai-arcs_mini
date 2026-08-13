#include <stdbool.h>
#include <stdint.h>
#include "ClockManager.h"
#include "log_print.h"
#include "ls_err.h"
#include "rtos_al.h"


static rtos_mutex s_luna_mutex;
static bool s_luna_ready;

static int32_t luna_demo_ensure_ready(void)
{
    if (s_luna_ready)
        return LS_OK;

    if (rtos_mutex_create(&s_luna_mutex))
        return LS_FAIL;

    s_luna_ready = true;

    return LS_OK;
}

void luna_demo_init(void)
{
    (void)luna_demo_ensure_ready();
}

int32_t luna_demo_get_version(uint32_t *version)
{
    if (version == NULL)
        return LS_ERR_PARAM;

    if (luna_demo_ensure_ready() != LS_OK)
        return LS_FAIL;

    rtos_mutex_lock(s_luna_mutex);
    *version = 0x1234;
    rtos_mutex_unlock(s_luna_mutex);

    CLOGD("%s\n", __func__);

    return LS_OK;
}

int32_t luna_demo_add(int32_t a, int32_t b, int32_t *result)
{
    int32_t ret;
    uint32_t i;

    if (result == NULL)
        return LS_ERR_PARAM;

    if (luna_demo_ensure_ready() != LS_OK)
        return LS_FAIL;

    rtos_mutex_lock(s_luna_mutex);

    *result = a + b;

    rtos_mutex_unlock(s_luna_mutex);
    CLOGD("%s\n", __func__);

    return LS_OK;
}

static RTOS_TASK_FCT(luna_demo_task)
{
    uint32_t version = 0;
    int32_t result = 0;

    (void)env;

    while (1)
    {
        if ((luna_demo_get_version(&version) == LS_OK) &&
            (luna_demo_add(10, 32, &result) == LS_OK))
        {
            CLOGD("luna demo: version=0x%08x add=%d\n", version, result);
        }
        else
        {
            CLOGE("luna demo failed\n");
        }

        rtos_task_suspend(5000);
    }
}

void luna_demo_task_start(void)
{
    rtos_task_create(luna_demo_task, "luna_demo", APPLICATION_TASK,
                     512, NULL, configMAX_PRIORITIES - 3, NULL);
}
