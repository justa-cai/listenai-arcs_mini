#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "stddef.h"

#include "cmd.h"

#include "shell.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_mem.h"
#include "power_manager.h"
#if defined(CONFIG_CHERRYUSB) && CONFIG_CHERRYUSB
#include "app_usb_cherry.h"
#endif

#define RECOVERY_REBOOT_TASK_STACK_SIZE 1024U
#define RECOVERY_REBOOT_DELAY_MS        20U

static int reboot_cmd_handler(int argc, char **argv)
{
    power_reboot_soft();

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, reboot,
                 reboot_cmd_handler, reboot);

static void recovery_reboot_task(void *arg)
{
    (void)arg;

    /* Let the one-shot ADB shell command return before closing its service. */
    vTaskDelay(pdMS_TO_TICKS(RECOVERY_REBOOT_DELAY_MS));

#if defined(CONFIG_CHERRYUSB) && CONFIG_CHERRYUSB
    app_usb_prepare_reboot();
#endif
    power_reboot_recovery();
}

static int recovery_cmd_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (xTaskCreate(recovery_reboot_task, "recovery", RECOVERY_REBOOT_TASK_STACK_SIZE, NULL,
                    configMAX_PRIORITIES - 1, NULL) != pdPASS) {
        printf("Failed to schedule recovery reboot\n");
        return -1;
    }

    return 0;
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
