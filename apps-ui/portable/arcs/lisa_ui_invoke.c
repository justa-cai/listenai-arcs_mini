#include <stdint.h>
#include <stddef.h>

#include "lisa_ui_workq.h"
#include "FreeRTOS.h"
#include "task.h"

#include "voice_msg.h"

static lisa_ui_workq_t workq_ui = NULL;
static lisa_ui_workq_t workq_bn = NULL;

int lisa_ui_invoke_init(void)
{
    workq_ui = lisa_ui_workq_create("worq.ui", 4096, 7, 128);

    assert(workq_ui);
}

int lisa_ui_invoke_ui_delayed(lisa_ui_workq_worker_t worker, void *arg, uint32_t len, uint32_t delay_ms)
{
    if (workq_ui == NULL) {
        return -1;
    }

    return lisa_ui_workq_submit_delay_ms(workq_ui, worker, arg, len, delay_ms);
}

int lisa_ui_invoke_bn_delayed(lisa_ui_workq_worker_t worker, void *arg, uint32_t len, uint32_t delay_ms)
{
    voice_invoke(worker, arg, len);
}
