#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "lisa_log.h"
#include "lisa_thread.h"
#include "lisa_mutex.h"

#include "flexible_button.h"

#define TAG "btn"

static lisa_mutex_t *btn_mutex = NULL;

static void lisa_btn_task(void *arg)
{
    LISA_LOGI(TAG, "lisa_btn_task enter");
    // lisa_thread_mdelay(2000);

    while (1) {
        lisa_mutex_lock(btn_mutex, LISA_OS_WAIT_FOREVER);
        flex_button_scan();
        lisa_mutex_unlock(btn_mutex);
        lisa_thread_mdelay(1000 / FLEX_BTN_SCAN_FREQ_HZ);
    }
}

void button_create(flex_button_t *btn)
{
    static bool is_init = false;

    if (!btn_mutex) {
        btn_mutex = lisa_mutex_create();
        if (btn_mutex == NULL) {
            LISA_LOGE(TAG, "btn_mutex lisa_mutex_create failed");
        }
        LISA_ASSERT(btn_mutex, "lisa btn mutex create failed");
    }

    lisa_mutex_lock(btn_mutex, LISA_OS_WAIT_FOREVER);
    flex_button_register(btn);
    lisa_mutex_unlock(btn_mutex);

    if (!is_init) {
        lisa_thread_attr_t attr = {
            .name = "btn",
            .stack_size = CONFIG_BUTTON_THREAD_STACK_SIZE,
            .priority = CONFIG_BUTTON_THREAD_PRIORITY,
        };
        lisa_thread_t *td = lisa_thread_create(&attr, lisa_btn_task, NULL);
        LISA_ASSERT(td, "lisa btn task create failed");

        is_init = true;
        LISA_LOGI(TAG, "lisa btn init done");
    }

}
