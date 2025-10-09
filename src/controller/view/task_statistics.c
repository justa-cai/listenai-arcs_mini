#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "lisa_log.h"
#include "FreeRTOS.h"
#include "task.h"

typedef struct {
    char pcTaskName[configMAX_TASK_NAME_LEN];
    uint32_t ulRunTimeCounter;
} TaskRunTimeStats_t;

#define MAX_TASKS 8 // 根据实际任务数调整

typedef struct {
    TaskRunTimeStats_t tasks[MAX_TASKS];
    uint32_t ulTotalTime;
    uint32_t uxTaskCount;
} RunTimeSnapshot_t;

// 获取运行时快照
static void prvGetRunTimeSnapshot(RunTimeSnapshot_t *snapshot)
{
    TaskStatus_t *pxTaskStatusArray;
    UBaseType_t uxArraySize;

    // 初始化
    snapshot->uxTaskCount = 0;
    snapshot->ulTotalTime = 0;

    uxArraySize = uxTaskGetNumberOfTasks();
    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray != NULL) {
        // 获取系统状态
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &snapshot->ulTotalTime);

        // 保存任务信息
        for (UBaseType_t x = 0; x < uxArraySize && x < MAX_TASKS; x++) {
            strncpy(snapshot->tasks[x].pcTaskName, pxTaskStatusArray[x].pcTaskName, configMAX_TASK_NAME_LEN - 1);
            snapshot->tasks[x].pcTaskName[configMAX_TASK_NAME_LEN - 1] = '\0';
            snapshot->tasks[x].ulRunTimeCounter = pxTaskStatusArray[x].ulRunTimeCounter;
        }
        snapshot->uxTaskCount = uxArraySize;

        vPortFree(pxTaskStatusArray);
    }
}

// 计算并打印时间段内的CPU使用率
static void vTaskGetPeriodRunTimeStats(const RunTimeSnapshot_t *start, const RunTimeSnapshot_t *end,
                                       char *pcWriteBuffer, size_t uxBufferLength)
{
    uint32_t ulTotalTimeDiff = end->ulTotalTime - start->ulTotalTime;
    size_t uxConsumedBufferLength = 0;

    // 避免除零错误
    if (ulTotalTimeDiff == 0) {
        snprintf(pcWriteBuffer, uxBufferLength, "Measurement period too short\n");
        return;
    }

    // 添加表头
    uxConsumedBufferLength = snprintf(pcWriteBuffer, uxBufferLength,
                                      "Task            Time(us)    CPU%%\n"
                                      "----------------------------------------\n");

    // 计算每个任务的统计信息
    for (UBaseType_t i = 0; i < end->uxTaskCount && uxConsumedBufferLength < uxBufferLength; i++) {
        // 查找起始快照中的对应任务
        uint32_t startCounter = 0;
        for (UBaseType_t j = 0; j < start->uxTaskCount; j++) {
            if (strcmp(end->tasks[i].pcTaskName, start->tasks[j].pcTaskName) == 0) {
                startCounter = start->tasks[j].ulRunTimeCounter;
                break;
            }
        }

        // 计算时间差和百分比
        uint32_t timeDiff = end->tasks[i].ulRunTimeCounter - startCounter;
        uint32_t percentage = (timeDiff * 100) / ulTotalTimeDiff;

        // 添加到输出缓冲区
        uxConsumedBufferLength +=
            snprintf(pcWriteBuffer + uxConsumedBufferLength, uxBufferLength - uxConsumedBufferLength,
                     "%-16s %-10lu %3lu%%\n", end->tasks[i].pcTaskName, timeDiff, percentage);
    }
}

// 使用示例
RunTimeSnapshot_t startSnapshot, endSnapshot;

void count_begin_run(void)
{
    prvGetRunTimeSnapshot(&startSnapshot);
}

void count_end_run(void)
{
    char buffer[1024];
    prvGetRunTimeSnapshot(&endSnapshot);
    vTaskGetPeriodRunTimeStats(&startSnapshot, &endSnapshot, buffer, sizeof(buffer));
    CLOGD("\n%s", buffer);
}

typedef struct {
    uint32_t ulRunTimeCounter_task_ui;
    uint32_t ulRunTimeCounter_lvgl_flush;
    uint32_t ulTotalRunTime;
} lvgl_task_stats_t;

TaskHandle_t task_ui;
TaskHandle_t lvgl_flush;

uint8_t lvgl_busy = 0;

lvgl_task_stats_t lvgl_count_start, lvgl_count_end;
void get_task_handle(void)
{
    task_ui = xTaskGetHandle("task_ui");
    configASSERT(task_ui);

    lvgl_flush = xTaskGetHandle("lvgl_flush");
    configASSERT(lvgl_flush);

    CLOGD("task_ui: %x, lvgl_flush: %x\n", task_ui, lvgl_flush);
}

static void lvgl_task_get_time_counter(lvgl_task_stats_t *stats)
{
    stats->ulRunTimeCounter_task_ui = ulTaskGetRunTimeCounter(task_ui);
    stats->ulRunTimeCounter_lvgl_flush = ulTaskGetRunTimeCounter(lvgl_flush);
    stats->ulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
}

static uint8_t lvgl_task_get_period_counter_per_task(lvgl_task_stats_t *start, lvgl_task_stats_t *end)
{
    uint32_t ulTotalTimeDiff = 0;
    uint32_t lvgl_task_ui_time_diff = 0;
    uint32_t lvgl_flush_time_diff = 0;

    if (end->ulTotalRunTime < start->ulTotalRunTime) {
        ulTotalTimeDiff = 4294967296 - start->ulTotalRunTime + end->ulTotalRunTime;
    } else {
        ulTotalTimeDiff = end->ulTotalRunTime - start->ulTotalRunTime;
    }

    if (end->ulRunTimeCounter_task_ui < start->ulRunTimeCounter_task_ui) {
        lvgl_task_ui_time_diff = 4294967296 - start->ulRunTimeCounter_task_ui + end->ulRunTimeCounter_task_ui;
    } else {
        lvgl_task_ui_time_diff = end->ulRunTimeCounter_task_ui - start->ulRunTimeCounter_task_ui;
    }

    if (end->ulRunTimeCounter_lvgl_flush < start->ulRunTimeCounter_lvgl_flush) {
        lvgl_flush_time_diff = 4294967296 - start->ulRunTimeCounter_lvgl_flush + end->ulRunTimeCounter_lvgl_flush;
    } else {
        lvgl_flush_time_diff = end->ulRunTimeCounter_lvgl_flush - start->ulRunTimeCounter_lvgl_flush;
    }

    // 避免除零错误
    if (ulTotalTimeDiff == 0) {
        CLOGD("Measurement period too short\n");
        return 0;
    }

    LOGI("lvgl_task_ui_time_diff %d lvgl_flush_time_diff %d total %d", lvgl_task_ui_time_diff, lvgl_flush_time_diff,
         ulTotalTimeDiff);

    int cpu_usage = (lvgl_task_ui_time_diff + lvgl_flush_time_diff) * 100 / ulTotalTimeDiff;
    if (cpu_usage > 100) {
        CLOGD("usage: %d > 100, ui_diff: %u, lvgl_flush_diff: %u, ulTotalTimeDiff: %u\nstart_ui: %u, start_flush: %u, "
              "start_total: %u\nend_ui: %u, end_flush: %u, end_total: %u\n",
              cpu_usage, lvgl_task_ui_time_diff, lvgl_flush_time_diff, ulTotalTimeDiff, start->ulRunTimeCounter_task_ui,
              start->ulRunTimeCounter_lvgl_flush, start->ulTotalRunTime, end->ulRunTimeCounter_task_ui,
              end->ulRunTimeCounter_lvgl_flush, end->ulTotalRunTime);
    }

    return cpu_usage;
}
void lvgl_task_count_start(void)
{
    lvgl_task_get_time_counter(&lvgl_count_start);
}

uint8_t lvgl_task_count_end(void)
{
    lvgl_task_get_time_counter(&lvgl_count_end);

    lvgl_busy = lvgl_task_get_period_counter_per_task(&lvgl_count_start, &lvgl_count_end);
    // CLOGD("lvgl busy: %d\n", lvgl_busy);

    return lvgl_busy;
}

#define LVGL_STATS_PERIOD_MS   99 // 统计周期500MS
#define MS_TO_RUNTIME_TICKS(x) ((x * configTICK_RATE_HZ / 1000) * (configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ))

static bool first_call = true;
static uint32_t period_start = 0;

void lvgl_task_cpu_percent_peroid500ms(void)
{
    if (portGET_RUN_TIME_COUNTER_VALUE() - period_start >= MS_TO_RUNTIME_TICKS(LVGL_STATS_PERIOD_MS)) {

        if (!first_call) {
            lvgl_task_get_time_counter(&lvgl_count_end);

            lvgl_busy = lvgl_task_get_period_counter_per_task(&lvgl_count_start, &lvgl_count_end);
            // CLOGI("lvgl busy: %d\n", lvgl_busy);

        } else {
            get_task_handle();
            int lvgl_test_period = MS_TO_RUNTIME_TICKS(LVGL_STATS_PERIOD_MS);
            CLOGD("first call, lvgl_test_period: %d\n", lvgl_test_period);
            first_call = false;
        }

        period_start = portGET_RUN_TIME_COUNTER_VALUE();
        lvgl_task_get_time_counter(&lvgl_count_start);
    }
}