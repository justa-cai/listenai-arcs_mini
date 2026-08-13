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
#include "power_manager.h"
#if defined(CONFIG_CHERRYUSB) && CONFIG_CHERRYUSB
#include "app_usb_cherry.h"
#endif

#define BOOT_INFO_REBOOT_CNT_MASK     0x000000FFu
#define BOOT_INFO_RECOVER_REASON_MASK 0x0000FF00u
#define BOOT_INFO_RECOVER_REASON_SOFT 0x00000100u
#define BOOT_INFO_HANDSHAKE_TIMEOUT   (1u << 29)
#define BOOT_INFO_WDT_FLAG            (1u << 30)
#define BOOT_INFO_RECOVERY_REQ        (1u << 31)
#define SYSTEM_SW_RESET_KEY           0xCAFE000Au

static int reboot_cmd_handler(int argc, char **argv)
{
    power_reboot_soft();

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, reboot,
                 reboot_cmd_handler, reboot);

static int recovery_cmd_handler(int argc, char **argv)
{
#if defined(CONFIG_CHERRYUSB) && CONFIG_CHERRYUSB
    app_usb_prepare_reboot();
#endif

    /*
     * 软重启会保存AON寄存器,
     * 为避免在boot引导时错误判断看门狗复位而进入恢复模式
     * 这里主动清除复位原因寄存器以及看门狗重启计数器
     */
    uint32_t reset_cause = IP_AON_CTRL->REG_SYSRST_STATUS.all;
    IP_AON_CTRL->REG_SYSRST_STATUS.all = reset_cause;
    (void)IP_AON_CTRL->REG_SYSRST_STATUS.all;

    /* 旧 boot 检查 req，新 boot 还会检查 recover_reason。 */
    uint32_t boot_info = IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    boot_info &= ~(BOOT_INFO_REBOOT_CNT_MASK | BOOT_INFO_RECOVER_REASON_MASK |
                   BOOT_INFO_HANDSHAKE_TIMEOUT | BOOT_INFO_WDT_FLAG | BOOT_INFO_RECOVERY_REQ);
    boot_info |= BOOT_INFO_RECOVER_REASON_SOFT | BOOT_INFO_RECOVERY_REQ;
    IP_AON_CTRL->REG_AON_DIG_RSVD4.all = boot_info;
    (void)IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    __RWMB();

    // turn off GPIOB_03 FORCE OUTPUT
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all &= ~(0b1111 << 21);
    // turn on GPIOB_03 FORCE OUTPUT HIGH
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all |= (0b1110 << 21);
    (void)IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all;
    __RWMB();

    /*
     * 不依赖 AP 的 IPC halt 应答。循环重试软复位，避免总线写入延迟或
     * 寄存器并发更新时单次触发未生效。
     */
    while (1) {
        IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CMN_RST_EN = 1;
        IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CP_RST_EN = 1;
        IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2AP_RST_EN = 1;
        __RWMB();
        IP_SYSCTRL->REG_SW_RESET_CP0.all = SYSTEM_SW_RESET_KEY;
        __RWMB();
    }
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, recovery,
                 recovery_cmd_handler, recovery);

static int threads_cmd(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();
    uint32_t tasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *item = lisa_mem_alloc(tasks * sizeof(TaskStatus_t));
    if (item) {
        uint32_t total = 0;
        tasks = uxTaskGetSystemState(item, tasks, &total);
        if (total > 0) {
            shellPrint(shell, "%s", "\n---------------------------------------------------------------------------------------------\n");
            shellPrint(shell, "%s", "Name                      State  Prio  Stack  MinFree    MaxUsed    Tid    Call100US      PCT\n");
            shellPrint(shell, "%s", "---------------------------------------------------------------------------------------------\n");
            for (uint32_t i = 0, pct = 0; i < tasks; i++) {
                uint32_t stack_size = (item[i].pxEndOfStack - item[i].pxStackBase + 2) * sizeof(StackType_t);
                uint32_t min_free = item[i].usStackHighWaterMark * sizeof(StackType_t);
                float max_used_pct = 100.0f - (float)min_free / (float)stack_size * 100.0f;
                
                if ((pct = (uint32_t)(100.0f * item[i].ulRunTimeCounter / total))) {
                    shellPrint(shell, "%-25s %-6c %-6u %-6u %-10u %-10.1f %-6u %-12u %5u%%\n",
                               item[i].pcTaskName, "XRBSD"[item[i].eCurrentState], item[i].uxCurrentPriority,
                               stack_size, min_free, max_used_pct, item[i].xTaskNumber,
                               item[i].ulRunTimeCounter, pct);
                } else {
                    shellPrint(shell, "%-25s %-6c %-6u %-6u %-10u %-10.1f %-6u %-12u %5s%%\n",
                               item[i].pcTaskName, "XRBSD"[item[i].eCurrentState], item[i].uxCurrentPriority,
                               stack_size, min_free, max_used_pct, item[i].xTaskNumber,
                               item[i].ulRunTimeCounter, "<1");
                }
            }
            shellPrint(shell, "%s", "---------------------------------------------------------------------------------------------\n\n");
        }
        lisa_mem_free(item);
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, threads,
                 threads_cmd, show threads info);
