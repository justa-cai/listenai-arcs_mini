#include "stdio.h"
#include <string.h>
#include <stdbool.h>
// #include <assert.h>

#include "arcs_ap.h"

// module: ic
#include "ic_mutex.h"
#include "ic_fence.h"
#include "ic_message.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define ASSERT(a) \
    if ((a) != true) { \
        printf("ASSERT: %s\n", __FUNCTION__, __LINE__); \
        while(1); \
    }

// 全局: 核间资源. ICFence id: 1
static IC_Mutex shared_ic_mutex;
static ICFenceHandle ic_fence_1;
volatile int gCounter = 0;

void ic_mutex_task(void *param)
{
    int ret;

    printf("ic_mutex_task enter...\n");

    IC_Mutex_init(&shared_ic_mutex, IC_MUTEX_SLEEP_WAIT, IC_MUTEX_TYPE_5);

    // ICFence id:1 - 本测试程序使用
    ic_fence_1 = ICFence_Creator_createObj(1);
    ASSERT(ic_fence_1 != NULL);

    // 对象级的核间同步, 以确认对端已经就绪.
    ICFence_syncWithRemote(ic_fence_1);

    ICFence_notify(ic_fence_1, (uint32_t)(uintptr_t) &gCounter);


    while (1) {
        ret = IC_Mutex_acquire(&shared_ic_mutex);
        ASSERT(ret == IC_OK);

        // 默认: memory_order_seq_cst
        gCounter ++;
        printf("gCounter == %d\n", gCounter);
        
        // atomic_fetch_add_explicit(&gCounter, 1, memory_order_seq_cst);
        
        ret = IC_Mutex_release(&shared_ic_mutex);
        ASSERT(ret == IC_OK);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        
    }
}

int main(int argc, char **argv)
{
    printf("AP Hard ID: %d\n", CONFIG_HARTID);

    CLOGD("aaaaa\n");

#if CONFIG_ARCS_AP_CORE
    #define MEM_CP_FLASH_BASE  (0x30d00000)
    #define MEM_CP_FLASH_SIZE  (0x300000)
    printf("boot cp from address: 0x%x\n", MEM_CP_FLASH_BASE);
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
#endif

    ic_message_init();
    printf("ic_message_init done!\n");

    xTaskCreate(ic_mutex_task, "ic_mutex_task", 4096, NULL, 6, NULL);

    return 0;
}
