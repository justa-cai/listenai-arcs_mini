// 最小 AP 固件（LLM 分支）：释放 CP 核 + IPC 骨架，随后空闲。
// 语音唤醒算法链路已被 CP 侧按键方案替代，AP 不再加载任何算法资源。

#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "arcs_ap.h"
#include "memap.h"
#include "ic_message.h"

#define TAG "ap"
#include "lisa_log.h"

/* CP 镜像 flash 基址契约：必须与 apps/arcs-mini prj.conf 的
 * CONFIG_MEM_FLASH_BASE 一致，防止 AP/CP 分割错配后静默跑飞 */
#ifndef EXPECTED_CP_FLASH_BASE
#define EXPECTED_CP_FLASH_BASE 0x30600000u
#endif
_Static_assert(MEM_CP_FLASH_BASE == EXPECTED_CP_FLASH_BASE,
               "memap.h MEM_CP_FLASH_BASE mismatches CP firmware");

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    LISA_LOGI(TAG, "minimal AP up, hart=%d, releasing CP @0x%x",
              CONFIG_HARTID, MEM_CP_FLASH_BASE);

    /* CP 核由本应用释放（boot 只把 AP 应用调起在 hart0；原厂 ap.bin 的
     * "boot cp from address" 即此逻辑）。IPC 共享内存由 ipc_auto_init
     * （PRE_DEVICES_INIT）先于 main 发布，CP 起来后 master 等到 pattern
     * 即完成握手——共享区地址必须与 CP 完全一致。 */
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
    ic_message_init();
    LISA_LOGI(TAG, "ic_message ready, AP idle");

    uint32_t loops = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        LISA_LOGI(TAG, "idle %u", (unsigned)++loops);
    }
    return 0;
}
