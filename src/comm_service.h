/**
 * CP & AP Common Service
 * @date 2025.04.16
 */

#ifndef __LISTENAI_COMM_SERVICE_H__
#define __LISTENAI_COMM_SERVICE_H__

#include <stdint.h>

#define _PACKED_ __attribute__((packed))

typedef enum
{
    FUNC_IVW_START_E = 1,
    FUNC_IVW_STATUS_E,
    FUNC_IVW_RESULT_E,
    FUNC_IVW_TIMEOUT_E,
    FUNC_IVW_CTRL_CMD_E,
} ivw_func_e;

typedef enum {
    e_algo_init_cmd,
    e_algo_timeout_set,
    e_algo_cmd_get,
    e_algo_workmode_set,
    e_algo_stage_set,
    e_algo_idle_set,
    e_algo_run_set,
    e_algo_gain_set,
    e_algo_cmd_count
} algo_cmd_msg_e;

typedef struct
{
    uint8_t cmd;
	uint8_t data[8];
} _PACKED_ ivw_item_t;

typedef struct
{
    uint32_t func;
    union
    {
        ivw_item_t ivw;
    };
} __attribute__((aligned(32))) func_descriptors_t;

/**
 * Common service init
 */
void comm_service_init();

void cp2ap_msg_send();

void lis_ivw_idle(void);

void lis_ivw_run(void);
void lis_ivw_gain_set(int8_t al, int8_t ar, int8_t dl, int8_t dr);
#endif