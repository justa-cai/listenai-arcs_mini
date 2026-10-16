// LLM 服务固件（AP 核）：释放 CP 后启动 LLM 服务，等待 IPC 请求。
// 模型/分词器通过 .incbin 编入固件，上电自动加载，无需运行时烧录。

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
#define EXPECTED_CP_FLASH_BASE 0x30700000u
#endif
_Static_assert(MEM_CP_FLASH_BASE == EXPECTED_CP_FLASH_BASE,
               "memap.h MEM_CP_FLASH_BASE mismatches CP firmware");

int llm_service_init(void);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    LISA_LOGI(TAG, "LLM service AP up, hart=%d, releasing CP @0x%x",
              CONFIG_HARTID, MEM_CP_FLASH_BASE);

    /* CP 核释放：入口指向 CP app 分区（0x700000），魔数放行。
     * IPC 共享内存由 ipc_auto_init（PRE_DEVICES_INIT）先于 main 发布。 */
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;

    ic_message_init();
    LISA_LOGI(TAG, "ic_message ready");

    /* LLM 服务：加载嵌入模型 → 监听 IPC 请求 */
    llm_service_init();

    uint32_t loops = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        LISA_LOGI(TAG, "idle %u", (unsigned)++loops);
    }
    return 0;
}
