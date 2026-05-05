/**
 * @file modem_dispatcher.h
 * @brief Shared dispatcher for serialized AT-backed modem socket service
 */

#ifndef LISA_MODEM_DRIVERS_COMMON_MODEM_DISPATCHER_H
#define LISA_MODEM_DRIVERS_COMMON_MODEM_DISPATCHER_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct modem_dispatcher modem_dispatcher_t;

typedef enum {
    MODEM_DISPATCHER_CONTROL_CONNECT = 0,
    MODEM_DISPATCHER_CONTROL_CLOSE,
    MODEM_DISPATCHER_CONTROL_DNS_RESOLVE,
    MODEM_DISPATCHER_CONTROL_SIGNAL_QUALITY,
    MODEM_DISPATCHER_CONTROL_UPDATE,
    MODEM_DISPATCHER_CONTROL_RECOVERY,
} modem_dispatcher_control_kind_t;

typedef enum {
    MODEM_DISPATCHER_WORK_COMPLETE = 0,
    MODEM_DISPATCHER_WORK_REQUEUE = 1,
    MODEM_DISPATCHER_WORK_WAIT = 2,
    MODEM_DISPATCHER_WORK_DROP = -1,
} modem_dispatcher_work_result_t;

typedef enum {
    MODEM_DISPATCHER_SERVICE_IDLE = 0,
    MODEM_DISPATCHER_SERVICE_CONTROL,
    MODEM_DISPATCHER_SERVICE_TX,
    MODEM_DISPATCHER_SERVICE_RX,
    MODEM_DISPATCHER_SERVICE_WAIT,
} modem_dispatcher_service_result_t;

typedef struct {
    modem_dispatcher_control_kind_t kind;
    int endpoint_id;
    uint32_t generation;
    uintptr_t value0;
    uintptr_t value1;
    void *context;
} modem_dispatcher_control_request_t;

typedef struct {
    bool (*generation_matches)(void *user_data, int endpoint_id, uint32_t generation);
    modem_dispatcher_work_result_t (*handle_control)(void *user_data,
                                                     const modem_dispatcher_control_request_t *request);
    modem_dispatcher_work_result_t (*handle_tx)(void *user_data, int endpoint_id, uint32_t generation);
    modem_dispatcher_work_result_t (*handle_rx)(void *user_data, int endpoint_id, uint32_t generation);
} modem_dispatcher_ops_t;

typedef struct {
    uint8_t max_endpoints;
    uint8_t control_queue_capacity;
    const modem_dispatcher_ops_t *ops;
    void *user_data;
    const char *task_name;
    uint16_t task_stack_size;
    uint8_t task_priority;
    uint16_t loop_delay_ms;
} modem_dispatcher_config_t;

modem_dispatcher_t *modem_dispatcher_create(const modem_dispatcher_config_t *config);
void modem_dispatcher_destroy(modem_dispatcher_t *dispatcher);

bool modem_dispatcher_start(modem_dispatcher_t *dispatcher);
void modem_dispatcher_stop(modem_dispatcher_t *dispatcher);

bool modem_dispatcher_enqueue_control(modem_dispatcher_t *dispatcher,
                                      const modem_dispatcher_control_request_t *request);
bool modem_dispatcher_mark_tx_ready(modem_dispatcher_t *dispatcher, int endpoint_id, uint32_t generation);
bool modem_dispatcher_mark_rx_ready(modem_dispatcher_t *dispatcher, int endpoint_id, uint32_t generation);

modem_dispatcher_service_result_t modem_dispatcher_service_once(modem_dispatcher_t *dispatcher);

int modem_dispatcher_set_loop_delay(modem_dispatcher_t *dispatcher, uint16_t delay_ms);
uint16_t modem_dispatcher_get_loop_delay(modem_dispatcher_t *dispatcher);

#ifdef __cplusplus
}
#endif

#endif
