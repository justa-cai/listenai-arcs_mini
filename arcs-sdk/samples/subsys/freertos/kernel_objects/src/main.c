/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief FreeRTOS kernel object validation sample.
 */

#define LOG_TAG "freertos_test"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

#define TEST_TIMEOUT_MS        1000U
#define TEST_TIMEOUT_TICKS     pdMS_TO_TICKS(TEST_TIMEOUT_MS)
#define TEST_TASK_STACK_WORDS  1024U
#define TEST_TASK_PRIO_LOW     (tskIDLE_PRIORITY + 1U)
#define TEST_TASK_PRIO_MID     (tskIDLE_PRIORITY + 2U)
#define TEST_TASK_PRIO_HIGH    (tskIDLE_PRIORITY + 3U)

#define TASK_ID_LOW            1U
#define TASK_ID_HIGH           2U
#define EVENT_TEST_BIT         ((EventBits_t)(1UL << 0))
#define MUTEX_WORKER_COUNT     3U
#define MUTEX_LOOP_COUNT       12U

typedef bool (*test_case_fn_t)(void);

typedef struct {
    SemaphoreHandle_t start_sem;
    QueueHandle_t order_queue;
    uint32_t id;
} task_order_ctx_t;

typedef struct {
    uint32_t sequence;
    uint32_t value;
} queue_payload_t;

typedef struct {
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t done_sem;
    volatile uint32_t counter;
    volatile bool failed;
} mutex_test_ctx_t;

typedef struct {
    EventGroupHandle_t event_group;
    EventBits_t bits;
} event_test_ctx_t;

static mutex_test_ctx_t g_mutex_ctx;

static void finish_current_test_task(void)
{
    vTaskDelete(NULL);

    /*
     * Some RISC-V ports request the self-delete context switch via software IRQ.
     * Do not let the task function return if that IRQ is not taken immediately.
     */
    for (;;) {
        __asm volatile("nop");
    }
}

static void task_order_worker(void *arg)
{
    task_order_ctx_t *ctx = (task_order_ctx_t *)arg;

    if (xSemaphoreTake(ctx->start_sem, TEST_TIMEOUT_TICKS) == pdTRUE) {
        (void)xQueueSend(ctx->order_queue, &ctx->id, 0);
    }

    finish_current_test_task();
}

static void binary_sem_worker(void *arg)
{
    SemaphoreHandle_t sem = (SemaphoreHandle_t)arg;

    vTaskDelay(pdMS_TO_TICKS(20));
    (void)xSemaphoreGive(sem);

    finish_current_test_task();
}

static void mutex_worker(void *arg)
{
    (void)arg;

    for (uint32_t i = 0; i < MUTEX_LOOP_COUNT; i++) {
        if (xSemaphoreTake(g_mutex_ctx.mutex, TEST_TIMEOUT_TICKS) != pdTRUE) {
            g_mutex_ctx.failed = true;
            break;
        }

        uint32_t snapshot = g_mutex_ctx.counter;
        vTaskDelay(pdMS_TO_TICKS(1));
        g_mutex_ctx.counter = snapshot + 1U;

        if (xSemaphoreGive(g_mutex_ctx.mutex) != pdTRUE) {
            g_mutex_ctx.failed = true;
            break;
        }

        taskYIELD();
    }

    (void)xSemaphoreGive(g_mutex_ctx.done_sem);
    finish_current_test_task();
}

static void event_worker(void *arg)
{
    event_test_ctx_t *ctx = (event_test_ctx_t *)arg;

    vTaskDelay(pdMS_TO_TICKS(20));
    (void)xEventGroupSetBits(ctx->event_group, ctx->bits);

    finish_current_test_task();
}

static void timer_callback(TimerHandle_t timer)
{
    SemaphoreHandle_t sem = (SemaphoreHandle_t)pvTimerGetTimerID(timer);

    (void)xSemaphoreGive(sem);
}

static void notify_worker(void *arg)
{
    TaskHandle_t target_task = (TaskHandle_t)arg;

    xTaskNotifyGive(target_task);
    finish_current_test_task();
}

static bool run_case(const char *name, test_case_fn_t test_fn)
{
    LOGI("Running: %s", name);

    bool passed = test_fn();
    if (passed) {
        LOGI("PASS: %s", name);
    } else {
        LOGE("FAIL: %s", name);
    }

    return passed;
}

#define CHECK_TRUE(condition, reason)                         \
    do {                                                      \
        if (!(condition)) {                                   \
            LOGE("  check failed: %s", (reason));             \
            return false;                                     \
        }                                                     \
    } while (0)

static bool test_task_priority_order(void)
{
    SemaphoreHandle_t start_sem = xSemaphoreCreateCounting(2, 0);
    QueueHandle_t order_queue = xQueueCreate(2, sizeof(uint32_t));

    CHECK_TRUE(start_sem != NULL, "create task start semaphore");
    CHECK_TRUE(order_queue != NULL, "create task order queue");

    task_order_ctx_t low_ctx = {
        .start_sem = start_sem,
        .order_queue = order_queue,
        .id = TASK_ID_LOW,
    };
    task_order_ctx_t high_ctx = {
        .start_sem = start_sem,
        .order_queue = order_queue,
        .id = TASK_ID_HIGH,
    };

    CHECK_TRUE(xTaskCreate(task_order_worker, "task_low", TEST_TASK_STACK_WORDS,
                           &low_ctx, TEST_TASK_PRIO_LOW, NULL) == pdPASS,
               "create low priority task");
    CHECK_TRUE(xTaskCreate(task_order_worker, "task_high", TEST_TASK_STACK_WORDS,
                           &high_ctx, TEST_TASK_PRIO_HIGH, NULL) == pdPASS,
               "create high priority task");

    vTaskDelay(pdMS_TO_TICKS(20));
    CHECK_TRUE(xSemaphoreGive(start_sem) == pdTRUE, "release first task");
    CHECK_TRUE(xSemaphoreGive(start_sem) == pdTRUE, "release second task");

    uint32_t first = 0;
    uint32_t second = 0;
    CHECK_TRUE(xQueueReceive(order_queue, &first, TEST_TIMEOUT_TICKS) == pdTRUE,
               "receive first task order");
    CHECK_TRUE(xQueueReceive(order_queue, &second, TEST_TIMEOUT_TICKS) == pdTRUE,
               "receive second task order");
    CHECK_TRUE((first == TASK_ID_HIGH) && (second == TASK_ID_LOW),
               "higher priority task runs first");

    vQueueDelete(order_queue);
    vSemaphoreDelete(start_sem);

    return true;
}

static bool test_queue_fifo(void)
{
    QueueHandle_t queue = xQueueCreate(3, sizeof(queue_payload_t));
    CHECK_TRUE(queue != NULL, "create queue");

    const queue_payload_t tx[] = {
        {.sequence = 0U, .value = 0x11U},
        {.sequence = 1U, .value = 0x22U},
        {.sequence = 2U, .value = 0x33U},
    };

    for (uint32_t i = 0; i < 3U; i++) {
        CHECK_TRUE(xQueueSend(queue, &tx[i], TEST_TIMEOUT_TICKS) == pdTRUE,
                   "send queue payload");
    }

    for (uint32_t i = 0; i < 3U; i++) {
        queue_payload_t rx = {0};
        CHECK_TRUE(xQueueReceive(queue, &rx, TEST_TIMEOUT_TICKS) == pdTRUE,
                   "receive queue payload");
        CHECK_TRUE((rx.sequence == tx[i].sequence) && (rx.value == tx[i].value),
                   "queue preserves FIFO order and payload");
    }

    vQueueDelete(queue);

    return true;
}

static bool test_binary_semaphore(void)
{
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    CHECK_TRUE(sem != NULL, "create binary semaphore");

    CHECK_TRUE(xTaskCreate(binary_sem_worker, "bin_sem", TEST_TASK_STACK_WORDS,
                           sem, TEST_TASK_PRIO_HIGH, NULL) == pdPASS,
               "create binary semaphore worker");
    CHECK_TRUE(xSemaphoreTake(sem, TEST_TIMEOUT_TICKS) == pdTRUE,
               "take semaphore given by worker task");

    vSemaphoreDelete(sem);

    return true;
}

static bool test_counting_semaphore(void)
{
    SemaphoreHandle_t sem = xSemaphoreCreateCounting(3, 0);
    CHECK_TRUE(sem != NULL, "create counting semaphore");

    CHECK_TRUE(xSemaphoreGive(sem) == pdTRUE, "give first token");
    CHECK_TRUE(xSemaphoreGive(sem) == pdTRUE, "give second token");
    CHECK_TRUE(xSemaphoreTake(sem, 0) == pdTRUE, "take first token");
    CHECK_TRUE(xSemaphoreTake(sem, 0) == pdTRUE, "take second token");
    CHECK_TRUE(xSemaphoreTake(sem, 0) == pdFALSE, "empty counting semaphore blocks");

    vSemaphoreDelete(sem);

    return true;
}

static bool test_mutex(void)
{
    memset(&g_mutex_ctx, 0, sizeof(g_mutex_ctx));
    g_mutex_ctx.mutex = xSemaphoreCreateMutex();
    g_mutex_ctx.done_sem = xSemaphoreCreateCounting(MUTEX_WORKER_COUNT, 0);

    CHECK_TRUE(g_mutex_ctx.mutex != NULL, "create mutex");
    CHECK_TRUE(g_mutex_ctx.done_sem != NULL, "create mutex done semaphore");

    static const char *const task_names[MUTEX_WORKER_COUNT] = {
        "mtx_a",
        "mtx_b",
        "mtx_c",
    };

    uint32_t created = 0;
    for (uint32_t i = 0; i < MUTEX_WORKER_COUNT; i++) {
        if (xTaskCreate(mutex_worker, task_names[i], TEST_TASK_STACK_WORDS,
                        NULL, TEST_TASK_PRIO_MID, NULL) != pdPASS) {
            LOGE("  check failed: create mutex worker");
            break;
        }
        created++;
    }

    for (uint32_t i = 0; i < created; i++) {
        CHECK_TRUE(xSemaphoreTake(g_mutex_ctx.done_sem, pdMS_TO_TICKS(3000)) == pdTRUE,
                   "wait mutex worker completion");
    }

    CHECK_TRUE(created == MUTEX_WORKER_COUNT, "all mutex workers created");
    CHECK_TRUE(!g_mutex_ctx.failed, "workers acquired and released mutex");
    CHECK_TRUE(g_mutex_ctx.counter == (MUTEX_WORKER_COUNT * MUTEX_LOOP_COUNT),
               "mutex protects shared counter");

    vSemaphoreDelete(g_mutex_ctx.done_sem);
    vSemaphoreDelete(g_mutex_ctx.mutex);

    return true;
}

static bool test_recursive_mutex(void)
{
    SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
    CHECK_TRUE(mutex != NULL, "create recursive mutex");

    CHECK_TRUE(xSemaphoreTakeRecursive(mutex, TEST_TIMEOUT_TICKS) == pdTRUE,
               "take recursive mutex first level");
    CHECK_TRUE(xSemaphoreTakeRecursive(mutex, TEST_TIMEOUT_TICKS) == pdTRUE,
               "take recursive mutex second level");
    CHECK_TRUE(xSemaphoreGiveRecursive(mutex) == pdTRUE,
               "give recursive mutex second level");
    CHECK_TRUE(xSemaphoreGiveRecursive(mutex) == pdTRUE,
               "give recursive mutex first level");

    vSemaphoreDelete(mutex);

    return true;
}

static bool test_event_group(void)
{
    EventGroupHandle_t event_group = xEventGroupCreate();
    CHECK_TRUE(event_group != NULL, "create event group");

    event_test_ctx_t ctx = {
        .event_group = event_group,
        .bits = EVENT_TEST_BIT,
    };

    CHECK_TRUE(xTaskCreate(event_worker, "evt_set", TEST_TASK_STACK_WORDS,
                           &ctx, TEST_TASK_PRIO_HIGH, NULL) == pdPASS,
               "create event worker");

    EventBits_t bits = xEventGroupWaitBits(event_group, EVENT_TEST_BIT, pdTRUE,
                                           pdTRUE, TEST_TIMEOUT_TICKS);
    CHECK_TRUE((bits & EVENT_TEST_BIT) == EVENT_TEST_BIT,
               "wait for event bit from worker");

    vEventGroupDelete(event_group);

    return true;
}

static bool test_software_timer(void)
{
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    CHECK_TRUE(sem != NULL, "create timer semaphore");

    TimerHandle_t timer = xTimerCreate("sw_timer", pdMS_TO_TICKS(30), pdFALSE,
                                       sem, timer_callback);
    CHECK_TRUE(timer != NULL, "create software timer");
    CHECK_TRUE(xTimerStart(timer, TEST_TIMEOUT_TICKS) == pdPASS,
               "start software timer");
    CHECK_TRUE(xSemaphoreTake(sem, TEST_TIMEOUT_TICKS) == pdTRUE,
               "timer callback gives semaphore");

    (void)xTimerDelete(timer, TEST_TIMEOUT_TICKS);
    vSemaphoreDelete(sem);

    return true;
}

static bool test_task_notification(void)
{
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    CHECK_TRUE(current_task != NULL, "get current task handle");

    while (ulTaskNotifyTake(pdTRUE, 0) != 0U) {
        ;
    }

    CHECK_TRUE(xTaskCreate(notify_worker, "notify", TEST_TASK_STACK_WORDS,
                           current_task, TEST_TASK_PRIO_HIGH, NULL) == pdPASS,
               "create notification worker");
    CHECK_TRUE(ulTaskNotifyTake(pdTRUE, TEST_TIMEOUT_TICKS) == 1U,
               "receive direct-to-task notification");

    return true;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    LOGI("=== FreeRTOS kernel objects test START ===");

    bool all_passed = true;
    all_passed &= run_case("task create and priority order", test_task_priority_order);
    all_passed &= run_case("queue FIFO", test_queue_fifo);
    all_passed &= run_case("binary semaphore", test_binary_semaphore);
    all_passed &= run_case("counting semaphore", test_counting_semaphore);
    all_passed &= run_case("mutex", test_mutex);
    all_passed &= run_case("recursive mutex", test_recursive_mutex);
    all_passed &= run_case("event group", test_event_group);
    all_passed &= run_case("software timer", test_software_timer);
    all_passed &= run_case("task notification", test_task_notification);

    if (all_passed) {
        LOGI("=== FreeRTOS kernel objects test PASS ===");
    } else {
        LOGE("=== FreeRTOS kernel objects test FAIL ===");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return all_passed ? 0 : -1;
}
