#if CFG_BACK_TRACE
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "log_print.h"

#include "multi_heap.h"

#include "FreeRTOS.h"
#include "task.h"

#include "backtrace.h"

#define rvb_print(fmt, ...)   printf(fmt, ##__VA_ARGS__)
#define rvb_println(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)


void backtrace_stack_dump(uint32_t *data, size_t cnt)
{
    char line[128];
    int pos;

    for (size_t i = 0; i < cnt; i++) {
        if (i % 4 == 0) {
            pos = snprintf(line, sizeof(line), "%p: ", data);
        }
        pos += snprintf(line + pos, sizeof(line) - pos, "%08lx ", (unsigned long)*data++);
        if ((i % 4 == 3) || (i == cnt - 1)) {
            rvb_println("%s", line);
        }
    }
}

void backtrace_show_tasks_info(void)
{
    const char *task_state_str[] = {
        "running",
        "ready",
        "blocked",
        "suspended",
        "deleted",
        "invalid",
    };

    rvb_println("");
    rvb_println("**********************task info**********************");

    TaskStatus_t *pxTaskStatusArray;
    UBaseType_t uxArraySize, uxTask;
    configRUN_TIME_COUNTER_TYPE ulTotalRunTime;
    uxArraySize = uxTaskGetNumberOfTasks();

    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray != NULL) {
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

        rvb_print("%-12s\t%-12s\t%-12s\t%-12s\t%-12s\t%-12s\n", "name", "state", "prio", "stack(byte)", "max-used(%)", "cpu(%)");
        for (uxTask = 0; uxTask < uxArraySize; uxTask++) {
            uint32_t stack_size =
                (uint32_t)pxTaskStatusArray[uxTask].pxEndOfStack - (uint32_t)pxTaskStatusArray[uxTask].pxStackBase;
            rvb_print("%-12s\t%-12s\t%-12ld\t%-12ld\t%-12.2f\t%-12.2f\n", pxTaskStatusArray[uxTask].pcTaskName,
                      task_state_str[pxTaskStatusArray[uxTask].eCurrentState],
                      (unsigned long)pxTaskStatusArray[uxTask].uxCurrentPriority,
                      (unsigned long)stack_size, 100.0f - (float)pxTaskStatusArray[uxTask].usStackHighWaterMark * 4 / (float)stack_size * 100,
                      (float)pxTaskStatusArray[uxTask].ulRunTimeCounter / (float)ulTotalRunTime * 100);
        }

        vPortFree(pxTaskStatusArray);
    } else {
        rvb_print("Failed to allocate memory for task status array.\n");
    }
    rvb_println("");
}

static void heap_travel_cb(void *start, void *end, multi_heap_info_t *info)
{
    rvb_println("%-12p\t%-12p\t%-12d\t%-12d\t%-12d", start, end, info->total_free_bytes,
                info->total_allocated_bytes, info->minimum_free_bytes);
}

void backtrace_show_heap_info(void)
{
    void heap_caps_travel(void (*callback)(void *, void *, multi_heap_info_t *));
    rvb_println("**********************heap info**********************");
    rvb_println("%-12s\t%-12s\t%-12s\t%-12s\t%-12s", "start", "end", "free", "used", "free(min)");
    heap_caps_travel(heap_travel_cb);
    rvb_println("");
}
#endif
