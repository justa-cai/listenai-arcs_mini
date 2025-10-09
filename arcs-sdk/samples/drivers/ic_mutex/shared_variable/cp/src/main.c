#include "stdio.h"
#include <string.h>
#include <stdbool.h>

// module: ic
#include "ic_mutex.h"
#include "ic_fence.h"
#include "ic_proxy.h"
#include "ic_message.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#define ASSERT(a) \
    if ((a) != true) { \
        printf("ASSERT: %s\n", __FUNCTION__, __LINE__); \
        while(1); \
    }


// 全局: 核间资源. ICFence id: 1
static IC_Mutex shared_ic_mutex;
static ICFenceHandle ic_fence_1;
volatile int *gPtrCounter = NULL;

void ic_mutex_task(void *param)
{
    int ret;

    printf("ic_mutex_task enter...\n");
    IC_Mutex_init(&shared_ic_mutex, IC_MUTEX_SLEEP_WAIT, IC_MUTEX_TYPE_5);

    // 延时一些，等待server端的ICFence对象就绪
    vTaskDelay(pdMS_TO_TICKS(1000));

    // ICFence id:1 - 本测试程序使用
    // 此时server端的ICFence对象已就绪
    ic_fence_1 = (ICFenceHandle) IC_Proxy_getRemoteFence(1);

    // 对象级的核间同步, 以确认对端已经就绪.
    ICFence_syncWithRemote(ic_fence_1);

    ICFence_wait(ic_fence_1, (uint32_t*) &gPtrCounter);

    while (1) {
        ret = IC_Mutex_acquire(&shared_ic_mutex);
        ASSERT(ret == IC_OK);

        // 默认: memory_order_seq_cst
        (*gPtrCounter) ++;
        printf("gCounter == %d\n", *gPtrCounter);

        // atomic_fetch_add_explicit(&gCounter, 1, memory_order_seq_cst);

        ret = IC_Mutex_release(&shared_ic_mutex);
        ASSERT(ret == IC_OK);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(int argc, char **argv)
{
    printf("CP=======! Hard ID: %d\n", CONFIG_HARTID);

    vTaskDelay(pdMS_TO_TICKS(1000));

    ic_message_init();
    printf("ic_message_init done!\n");

    xTaskCreate(ic_mutex_task, "ic_mutex_task",4096, NULL, 6, NULL);

    return 0;
}
