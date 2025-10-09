#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "stddef.h"

#include "cmd.h"

#include "shell.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_mem.h"
#include "chip.h"

static int reboot_cmd_handler(int argc, char **argv)
{
    extern void sys_platform_sw_full_reset(void);
    sys_platform_sw_full_reset();

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, reboot,
                 reboot_cmd_handler, reboot);

static int recovery_cmd_handler(int argc, char **argv)
{
    struct boot_info {
        uint32_t reboot_cnt: 8;
        uint32_t recover_reason: 8;
        uint32_t reserved: 15;
        uint32_t req: 1;
    };

    /*
     * 软重启会保存AON寄存器,
     * 为避免在boot引导时错误判断看门狗复位而进入恢复模式
     * 这里主动清除复位原因寄存器以及看门狗重启计数器
     */
    uint32_t rstCause;
    rstCause = IP_AON_CTRL->REG_SYSRST_STATUS.all;
    IP_AON_CTRL->REG_SYSRST_STATUS.all = rstCause;
    uint32_t cause = IP_AON_CTRL->REG_SYSRST_STATUS.all;

    /* set recovery request */
    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    info->req = 1;
    info->reboot_cnt = 0;

    // turn off GPIOB_03 FORCE OUTPUT
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all &= ~(0b1111 << 21);
    // turn on GPIOB_03 FORCE OUTPUT HIGH
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all |= (0b1110 << 21);

    IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CMN_RST_EN = 1;
    IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CP_RST_EN = 1;
    IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2AP_RST_EN = 1;
    __COMPILER_BARRIER();
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, recovery,
                 recovery_cmd_handler, recovery);

static int threads_cmd(int argc, char **argv)
{
    uint32_t tasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *item = lisa_mem_alloc(tasks * sizeof(TaskStatus_t));
    if (item) {
        uint32_t total = 0;
        tasks = uxTaskGetSystemState(item, tasks, &total);
        if (total > 0) {
            printf("%s", "\n---------------------------------------------------------------------------------------------\n");
            printf("%s", "Name                      State  Prio  Stack  MinFree    MaxUsed    Tid    Call100US      PCT\n");
            printf("%s", "---------------------------------------------------------------------------------------------\n");
            for (uint32_t i = 0, pct = 0; i < tasks; i++) {
                uint32_t stack_size = (item[i].pxEndOfStack - item[i].pxStackBase + 2) * sizeof(StackType_t);
                uint32_t min_free = item[i].usStackHighWaterMark * sizeof(StackType_t);
                float max_used_pct = 100.0f - (float)min_free / (float)stack_size * 100.0f;
                
                if ((pct = (uint32_t)(100.0f * item[i].ulRunTimeCounter / total))) {
                    printf("%-25s %-6c %-6u %-6u %-10u %-10.1f %-6u %-12u %5u%%\n", item[i].pcTaskName,
                           "XRBSD"[item[i].eCurrentState], item[i].uxCurrentPriority,
                           stack_size, min_free, max_used_pct, item[i].xTaskNumber,
                           item[i].ulRunTimeCounter, pct);
                } else {
                    printf("%-25s %-6c %-6u %-6u %-10u %-10.1f %-6u %-12u %5s%%\n", item[i].pcTaskName,
                           "XRBSD"[item[i].eCurrentState], item[i].uxCurrentPriority,
                           stack_size, min_free, max_used_pct, item[i].xTaskNumber,
                           item[i].ulRunTimeCounter, "<1");
                }
            }
            printf("%s", "---------------------------------------------------------------------------------------------\n\n");
        }
        lisa_mem_free(item);
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, threads,
                 threads_cmd, show threads info);
