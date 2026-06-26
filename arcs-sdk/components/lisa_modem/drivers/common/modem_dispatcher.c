/**
 * @file modem_dispatcher.c
 * @brief Shared dispatcher for serialized AT-backed modem socket service
 */

#include "drivers/common/modem_dispatcher.h"
#include "at_mem.h"

#ifndef taskYIELD
#define taskYIELD() vTaskDelay(0)
#endif

#define MODEM_DISPATCHER_DEFAULT_CONTROL_CAPACITY 8U
#define MODEM_DISPATCHER_DEFAULT_TASK_NAME "modem_disp"
#define MODEM_DISPATCHER_DEFAULT_STACK_SIZE 4096U
#define MODEM_DISPATCHER_DEFAULT_PRIORITY 8U
#define MODEM_DISPATCHER_TX_BURST_MAX_TURNS 4U
#define MODEM_DISPATCHER_RX_BURST_MAX_TURNS 1U
/*
 * ML307 cached-mode TCP pulls can take ~150ms for a 4KB MIPRD under load.
 * Keep the burst window long enough for one backlog-heavy socket to sustain a
 * few consecutive pulls before TX fairness kicks back in.
 */
#define MODEM_DISPATCHER_RX_BURST_TIME_BUDGET_MS 100U
#define MODEM_DISPATCHER_WAIT_TICK_COUNT 1U

typedef struct {
    int endpoint_id;
    uint32_t generation;
} modem_dispatcher_endpoint_request_t;

typedef struct {
    modem_dispatcher_control_request_t *items;
    uint16_t capacity;
    uint16_t head;
    uint16_t size;
} modem_dispatcher_control_queue_t;

typedef struct {
    modem_dispatcher_endpoint_request_t *items;
    uint16_t capacity;
    uint16_t head;
    uint16_t size;
} modem_dispatcher_endpoint_queue_t;

struct modem_dispatcher {
    modem_dispatcher_config_t config;
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t wake_sem;
    TaskHandle_t task_handle;
    bool running;
    bool prefer_tx;
    uint8_t tx_burst_budget;
    bool rx_burst_active;
    uint8_t rx_burst_budget;
    TickType_t rx_burst_start_tick;
    modem_dispatcher_endpoint_request_t rx_burst_request;
    modem_dispatcher_control_queue_t control_queue;
    modem_dispatcher_endpoint_queue_t tx_queue;
    modem_dispatcher_endpoint_queue_t rx_queue;
};

static uint32_t modem_dispatcher_tick_elapsed_ms(TickType_t start, TickType_t end)
{
    return (uint32_t)((end - start) * portTICK_PERIOD_MS);
}

static void modem_dispatcher_clear_rx_burst(modem_dispatcher_t *dispatcher)
{
    if (!dispatcher) {
        return;
    }

    dispatcher->rx_burst_active = false;
    dispatcher->rx_burst_budget = 0U;
    dispatcher->rx_burst_start_tick = 0U;
    dispatcher->rx_burst_request.endpoint_id = -1;
    dispatcher->rx_burst_request.generation = 0U;
}

static bool modem_dispatcher_generation_matches(const modem_dispatcher_t *dispatcher, int endpoint_id,
                                                uint32_t generation)
{
    if (!dispatcher || !dispatcher->config.ops || !dispatcher->config.ops->generation_matches) {
        return true;
    }

    return dispatcher->config.ops->generation_matches(dispatcher->config.user_data, endpoint_id, generation);
}

static bool modem_dispatcher_control_push(modem_dispatcher_control_queue_t *queue,
                                          const modem_dispatcher_control_request_t *request)
{
    uint16_t tail;

    if (!queue || !queue->items || !request || queue->size >= queue->capacity) {
        return false;
    }

    tail = (uint16_t)((queue->head + queue->size) % queue->capacity);
    queue->items[tail] = *request;
    queue->size++;
    return true;
}

static bool modem_dispatcher_control_pop(modem_dispatcher_control_queue_t *queue,
                                         modem_dispatcher_control_request_t *request)
{
    if (!queue || !queue->items || !request || queue->size == 0U) {
        return false;
    }

    *request = queue->items[queue->head];
    queue->head = (uint16_t)((queue->head + 1U) % queue->capacity);
    queue->size--;
    return true;
}

static bool modem_dispatcher_endpoint_contains(const modem_dispatcher_endpoint_queue_t *queue,
                                               int endpoint_id, uint32_t generation)
{
    uint16_t i;

    if (!queue || !queue->items) {
        return false;
    }

    for (i = 0; i < queue->size; ++i) {
        uint16_t index = (uint16_t)((queue->head + i) % queue->capacity);

        if (queue->items[index].endpoint_id == endpoint_id &&
            queue->items[index].generation == generation) {
            return true;
        }
    }

    return false;
}

static bool modem_dispatcher_endpoint_push(modem_dispatcher_endpoint_queue_t *queue,
                                           int endpoint_id, uint32_t generation)
{
    uint16_t tail;

    if (!queue || !queue->items || queue->size >= queue->capacity) {
        return false;
    }
    if (modem_dispatcher_endpoint_contains(queue, endpoint_id, generation)) {
        return true;
    }

    tail = (uint16_t)((queue->head + queue->size) % queue->capacity);
    queue->items[tail].endpoint_id = endpoint_id;
    queue->items[tail].generation = generation;
    queue->size++;
    return true;
}

static bool modem_dispatcher_endpoint_pop(modem_dispatcher_endpoint_queue_t *queue,
                                          modem_dispatcher_endpoint_request_t *request)
{
    if (!queue || !queue->items || !request || queue->size == 0U) {
        return false;
    }

    *request = queue->items[queue->head];
    queue->head = (uint16_t)((queue->head + 1U) % queue->capacity);
    queue->size--;
    return true;
}

static void modem_dispatcher_wake(modem_dispatcher_t *dispatcher)
{
    if (dispatcher && dispatcher->wake_sem) {
        (void)xSemaphoreGive(dispatcher->wake_sem);
    }
}

/**
 * @brief Try to pop a control request from the queue (must be called with mutex held).
 * @return true if a valid request was popped into *out_request.
 */
static bool modem_dispatcher_pop_control(modem_dispatcher_t *dispatcher,
                                         modem_dispatcher_control_request_t *out_request)
{
    if (!dispatcher || !dispatcher->config.ops || !dispatcher->config.ops->handle_control) {
        return false;
    }

    return modem_dispatcher_control_pop(&dispatcher->control_queue, out_request);
}

/**
 * @brief Try to pop an endpoint request from the queue (must be called with mutex held).
 * @return true if a valid request was popped into *out_request.
 */
static bool modem_dispatcher_pop_endpoint(modem_dispatcher_t *dispatcher,
                                          modem_dispatcher_endpoint_queue_t *queue,
                                          modem_dispatcher_endpoint_request_t *out_request)
{
    if (!dispatcher || !queue) {
        return false;
    }

    while (queue->size > 0U) {
        if (!modem_dispatcher_endpoint_pop(queue, out_request)) {
            return false;
        }
        if (modem_dispatcher_generation_matches(dispatcher, out_request->endpoint_id,
                                                out_request->generation)) {
            return true;
        }
    }
    return false;
}

static bool modem_dispatcher_take_rx_burst(modem_dispatcher_t *dispatcher,
                                           modem_dispatcher_endpoint_request_t *out_request)
{
    if (!dispatcher || !out_request || !dispatcher->rx_burst_active) {
        return false;
    }

    if (dispatcher->rx_burst_budget == 0U ||
        !modem_dispatcher_generation_matches(dispatcher,
                                            dispatcher->rx_burst_request.endpoint_id,
                                            dispatcher->rx_burst_request.generation) ||
        modem_dispatcher_tick_elapsed_ms(dispatcher->rx_burst_start_tick,
                                         xTaskGetTickCount()) >=
            MODEM_DISPATCHER_RX_BURST_TIME_BUDGET_MS) {
        modem_dispatcher_clear_rx_burst(dispatcher);
        return false;
    }

    *out_request = dispatcher->rx_burst_request;
    dispatcher->rx_burst_budget--;
    dispatcher->prefer_tx = true;
    return true;
}

static void modem_dispatcher_keep_rx_burst(modem_dispatcher_t *dispatcher,
                                           const modem_dispatcher_endpoint_request_t *request)
{
    if (!dispatcher || !request || MODEM_DISPATCHER_RX_BURST_MAX_TURNS == 0U) {
        return;
    }

    if (!dispatcher->rx_burst_active ||
        dispatcher->rx_burst_request.endpoint_id != request->endpoint_id ||
        dispatcher->rx_burst_request.generation != request->generation) {
        dispatcher->rx_burst_active = true;
        dispatcher->rx_burst_budget = MODEM_DISPATCHER_RX_BURST_MAX_TURNS;
        dispatcher->rx_burst_start_tick = xTaskGetTickCount();
        dispatcher->rx_burst_request = *request;
    }
}

static void modem_dispatcher_finish_rx_burst(modem_dispatcher_t *dispatcher,
                                             const modem_dispatcher_endpoint_request_t *request)
{
    if (!dispatcher || !request || !dispatcher->rx_burst_active) {
        return;
    }

    if (dispatcher->rx_burst_request.endpoint_id == request->endpoint_id &&
        dispatcher->rx_burst_request.generation == request->generation) {
        modem_dispatcher_clear_rx_burst(dispatcher);
    }
}

static void modem_dispatcher_task_fn(void *user_data)
{
    modem_dispatcher_t *dispatcher = (modem_dispatcher_t *)user_data;

    while (dispatcher && dispatcher->running) {
        modem_dispatcher_service_result_t sr = modem_dispatcher_service_once(dispatcher);
        if (sr == MODEM_DISPATCHER_SERVICE_IDLE) {
            if (dispatcher->wake_sem) {
                (void)xSemaphoreTake(dispatcher->wake_sem, portMAX_DELAY);
            } else {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        } else if (sr == MODEM_DISPATCHER_SERVICE_WAIT) {
            if (dispatcher->wake_sem) {
                (void)xSemaphoreTake(dispatcher->wake_sem, MODEM_DISPATCHER_WAIT_TICK_COUNT);
            } else {
                vTaskDelay(MODEM_DISPATCHER_WAIT_TICK_COUNT);
            }
        } else {
            taskYIELD();
        }
        vTaskDelay(3);
    }

    vTaskDelete(NULL);
}

modem_dispatcher_t *modem_dispatcher_create(const modem_dispatcher_config_t *config)
{
    modem_dispatcher_t *dispatcher;
    uint16_t control_capacity;
    uint16_t data_capacity;

    if (!config || !config->ops || config->max_endpoints == 0U) {
        return NULL;
    }

    control_capacity = config->control_queue_capacity > 0U
                     ? config->control_queue_capacity
                     : MODEM_DISPATCHER_DEFAULT_CONTROL_CAPACITY;
    data_capacity = config->max_endpoints;

    dispatcher = (modem_dispatcher_t *)at_mem_calloc(1, sizeof(*dispatcher));
    if (!dispatcher) {
        return NULL;
    }

    dispatcher->control_queue.items = (modem_dispatcher_control_request_t *)
        at_mem_calloc(control_capacity, sizeof(*dispatcher->control_queue.items));
    dispatcher->tx_queue.items = (modem_dispatcher_endpoint_request_t *)
        at_mem_calloc(data_capacity, sizeof(*dispatcher->tx_queue.items));
    dispatcher->rx_queue.items = (modem_dispatcher_endpoint_request_t *)
        at_mem_calloc(data_capacity, sizeof(*dispatcher->rx_queue.items));
    dispatcher->mutex = xSemaphoreCreateMutex();
    dispatcher->wake_sem = xSemaphoreCreateBinary();

    if (!dispatcher->control_queue.items || !dispatcher->tx_queue.items || !dispatcher->rx_queue.items ||
        !dispatcher->mutex || !dispatcher->wake_sem) {
        modem_dispatcher_destroy(dispatcher);
        return NULL;
    }

    dispatcher->config = *config;
    dispatcher->config.control_queue_capacity = (uint8_t)control_capacity;
    dispatcher->config.task_name = config->task_name ? config->task_name : MODEM_DISPATCHER_DEFAULT_TASK_NAME;
    dispatcher->config.task_stack_size = config->task_stack_size > 0U
                                       ? config->task_stack_size
                                       : MODEM_DISPATCHER_DEFAULT_STACK_SIZE;
    dispatcher->config.task_priority = config->task_priority > 0U
                                     ? config->task_priority
                                     : MODEM_DISPATCHER_DEFAULT_PRIORITY;
    dispatcher->control_queue.capacity = control_capacity;
    dispatcher->tx_queue.capacity = data_capacity;
    dispatcher->rx_queue.capacity = data_capacity;
    dispatcher->prefer_tx = true;
    dispatcher->tx_burst_budget = MODEM_DISPATCHER_TX_BURST_MAX_TURNS;
    modem_dispatcher_clear_rx_burst(dispatcher);
    return dispatcher;
}

void modem_dispatcher_destroy(modem_dispatcher_t *dispatcher)
{
    if (!dispatcher) {
        return;
    }

    modem_dispatcher_stop(dispatcher);
    if (dispatcher->wake_sem) {
        vSemaphoreDelete(dispatcher->wake_sem);
    }
    if (dispatcher->mutex) {
        vSemaphoreDelete(dispatcher->mutex);
    }
    at_mem_free(dispatcher->rx_queue.items);
    at_mem_free(dispatcher->tx_queue.items);
    at_mem_free(dispatcher->control_queue.items);
    at_mem_free(dispatcher);
}

bool modem_dispatcher_start(modem_dispatcher_t *dispatcher)
{
    BaseType_t ret;

    if (!dispatcher) {
        return false;
    }
    if (dispatcher->running) {
        return true;
    }

    dispatcher->running = true;
    ret = xTaskCreate(modem_dispatcher_task_fn, dispatcher->config.task_name,
                      dispatcher->config.task_stack_size, dispatcher,
                      dispatcher->config.task_priority, &dispatcher->task_handle);
    if (ret != pdPASS) {
        dispatcher->running = false;
        dispatcher->task_handle = NULL;
        return false;
    }

    return true;
}

void modem_dispatcher_stop(modem_dispatcher_t *dispatcher)
{
    if (!dispatcher || !dispatcher->running) {
        return;
    }

    dispatcher->running = false;
    modem_dispatcher_wake(dispatcher);
    if (dispatcher->task_handle) {
        vTaskDelete(dispatcher->task_handle);
        dispatcher->task_handle = NULL;
    }
}

bool modem_dispatcher_enqueue_control(modem_dispatcher_t *dispatcher,
                                      const modem_dispatcher_control_request_t *request)
{
    bool ok;

    if (!dispatcher || !request) {
        return false;
    }

    (void)xSemaphoreTake(dispatcher->mutex, portMAX_DELAY);
    ok = modem_dispatcher_control_push(&dispatcher->control_queue, request);
    (void)xSemaphoreGive(dispatcher->mutex);
    if (ok) {
        modem_dispatcher_wake(dispatcher);
    }
    return ok;
}

bool modem_dispatcher_mark_tx_ready(modem_dispatcher_t *dispatcher, int endpoint_id, uint32_t generation)
{
    bool ok;

    if (!dispatcher || endpoint_id < 0) {
        return false;
    }

    (void)xSemaphoreTake(dispatcher->mutex, portMAX_DELAY);
    ok = modem_dispatcher_endpoint_push(&dispatcher->tx_queue, endpoint_id, generation);
    (void)xSemaphoreGive(dispatcher->mutex);
    if (ok) {
        modem_dispatcher_wake(dispatcher);
    }
    return ok;
}

bool modem_dispatcher_mark_rx_ready(modem_dispatcher_t *dispatcher, int endpoint_id, uint32_t generation)
{
    bool ok;

    if (!dispatcher || endpoint_id < 0) {
        return false;
    }

    (void)xSemaphoreTake(dispatcher->mutex, portMAX_DELAY);
    ok = modem_dispatcher_endpoint_push(&dispatcher->rx_queue, endpoint_id, generation);
    (void)xSemaphoreGive(dispatcher->mutex);
    if (ok) {
        modem_dispatcher_wake(dispatcher);
    }
    return ok;
}

modem_dispatcher_service_result_t modem_dispatcher_service_once(modem_dispatcher_t *dispatcher)
{
    modem_dispatcher_service_result_t result = MODEM_DISPATCHER_SERVICE_IDLE;
    modem_dispatcher_control_request_t ctrl_req;
    modem_dispatcher_endpoint_request_t ep_req;
    bool is_tx = false;
    bool rx_pending;

    if (!dispatcher || !dispatcher->mutex) {
        return MODEM_DISPATCHER_SERVICE_IDLE;
    }

    /*
     * Pop a work item under the lock, then release the lock BEFORE executing
     * the handler callback.  This prevents a deadlock where:
     *   dispatcher task: holds dispatcher->mutex → at_client_send_cmd (holds
     *                    cmd_mutex, waits for uart_rx to process OK)
     *   uart_rx task:    URC callback → modem_dispatcher_mark_rx_ready →
     *                    waits for dispatcher->mutex
     */
    (void)xSemaphoreTake(dispatcher->mutex, portMAX_DELAY);

    /* 1. Try control queue first */
    if (modem_dispatcher_pop_control(dispatcher, &ctrl_req)) {
        (void)xSemaphoreGive(dispatcher->mutex);

        modem_dispatcher_work_result_t wr =
            dispatcher->config.ops->handle_control(dispatcher->config.user_data, &ctrl_req);
        if (wr == MODEM_DISPATCHER_WORK_REQUEUE || wr == MODEM_DISPATCHER_WORK_WAIT) {
            (void)xSemaphoreTake(dispatcher->mutex, portMAX_DELAY);
            (void)modem_dispatcher_control_push(&dispatcher->control_queue, &ctrl_req);
            (void)xSemaphoreGive(dispatcher->mutex);
        }
        return wr == MODEM_DISPATCHER_WORK_WAIT
             ? MODEM_DISPATCHER_SERVICE_WAIT
             : MODEM_DISPATCHER_SERVICE_CONTROL;
    }

    /*
     * 2. Prefer TX, but cap consecutive TX turns while RX is pending so
     * downstream prefetch cannot be starved by a continuous upload stream.
     */
    rx_pending = dispatcher->rx_burst_active || dispatcher->rx_queue.size > 0U;
    if (dispatcher->tx_queue.size > 0U &&
        (!rx_pending || dispatcher->tx_burst_budget > 0U) &&
        modem_dispatcher_pop_endpoint(dispatcher, &dispatcher->tx_queue, &ep_req)) {
        is_tx = true;
        if (rx_pending) {
            if (dispatcher->tx_burst_budget > 0U) {
                dispatcher->tx_burst_budget--;
            }
        } else {
            dispatcher->tx_burst_budget = MODEM_DISPATCHER_TX_BURST_MAX_TURNS;
        }
    } else if (modem_dispatcher_take_rx_burst(dispatcher, &ep_req)) {
        is_tx = false;
        dispatcher->tx_burst_budget = MODEM_DISPATCHER_TX_BURST_MAX_TURNS;
    } else if (dispatcher->prefer_tx) {
        if (modem_dispatcher_pop_endpoint(dispatcher, &dispatcher->rx_queue, &ep_req)) {
            is_tx = false;
            dispatcher->prefer_tx = true;
            dispatcher->tx_burst_budget = MODEM_DISPATCHER_TX_BURST_MAX_TURNS;
        } else {
            (void)xSemaphoreGive(dispatcher->mutex);
            return MODEM_DISPATCHER_SERVICE_IDLE;
        }
    } else {
        if (modem_dispatcher_pop_endpoint(dispatcher, &dispatcher->rx_queue, &ep_req)) {
            is_tx = false;
            dispatcher->prefer_tx = true;
            dispatcher->tx_burst_budget = MODEM_DISPATCHER_TX_BURST_MAX_TURNS;
        } else if (modem_dispatcher_pop_endpoint(dispatcher, &dispatcher->tx_queue, &ep_req)) {
            is_tx = true;
            dispatcher->prefer_tx = false;
            dispatcher->tx_burst_budget = MODEM_DISPATCHER_TX_BURST_MAX_TURNS;
        } else {
            (void)xSemaphoreGive(dispatcher->mutex);
            return MODEM_DISPATCHER_SERVICE_IDLE;
        }
    }

    /* Release lock before executing the (potentially blocking) handler */
    (void)xSemaphoreGive(dispatcher->mutex);

    result = is_tx ? MODEM_DISPATCHER_SERVICE_TX : MODEM_DISPATCHER_SERVICE_RX;

    modem_dispatcher_work_result_t wr;
    if (is_tx) {
        wr = dispatcher->config.ops->handle_tx(dispatcher->config.user_data,
                                               ep_req.endpoint_id, ep_req.generation);
    } else {
        wr = dispatcher->config.ops->handle_rx(dispatcher->config.user_data,
                                               ep_req.endpoint_id, ep_req.generation);
    }

    if (wr == MODEM_DISPATCHER_WORK_REQUEUE || wr == MODEM_DISPATCHER_WORK_WAIT) {
        (void)xSemaphoreTake(dispatcher->mutex, portMAX_DELAY);
        if (is_tx) {
            (void)modem_dispatcher_endpoint_push(&dispatcher->tx_queue,
                                                 ep_req.endpoint_id, ep_req.generation);
            if (dispatcher->rx_queue.size > 0U || dispatcher->rx_burst_active) {
                dispatcher->prefer_tx = false;
            }
        } else {
            bool continue_burst = dispatcher->control_queue.size == 0U &&
                                  dispatcher->rx_burst_budget > 0U &&
                                  modem_dispatcher_tick_elapsed_ms(dispatcher->rx_burst_start_tick,
                                                                   xTaskGetTickCount()) <
                                      MODEM_DISPATCHER_RX_BURST_TIME_BUDGET_MS;

            if (!dispatcher->rx_burst_active) {
                modem_dispatcher_keep_rx_burst(dispatcher, &ep_req);
                continue_burst = dispatcher->rx_burst_budget > 0U;
            }

            if (!continue_burst) {
                modem_dispatcher_finish_rx_burst(dispatcher, &ep_req);
                (void)modem_dispatcher_endpoint_push(&dispatcher->rx_queue,
                                                     ep_req.endpoint_id, ep_req.generation);
                if (dispatcher->tx_queue.size > 0U) {
                    dispatcher->prefer_tx = true;
                }
            }
        }

        (void)xSemaphoreGive(dispatcher->mutex);
    } else if (!is_tx) {
        (void)xSemaphoreTake(dispatcher->mutex, portMAX_DELAY);
        modem_dispatcher_finish_rx_burst(dispatcher, &ep_req);
        (void)xSemaphoreGive(dispatcher->mutex);
    }

    return wr == MODEM_DISPATCHER_WORK_WAIT ? MODEM_DISPATCHER_SERVICE_WAIT : result;
}
