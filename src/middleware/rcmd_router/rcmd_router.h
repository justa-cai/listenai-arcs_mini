#ifndef CMD_ROUTER_H
#define CMD_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "voice_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
    RCMD_ROUTER_CMD_TYPE_OFFLINE = 0,
    RCMD_ROUTER_CMD_TYPE_ONLINE
}rcmd_router_cmd_type_e;

typedef enum{
    RCMD_ROUTER_STATUS_SESSION_FINISH = 0,

}rcmd_router_status_e;

typedef void (*rcmd_router_action_t)(voice_msg_cloud_recognized_command_t *params, rcmd_router_cmd_type_e cmd_type);
typedef void (*rcmd_router_status_action_t)(rcmd_router_status_e status,rcmd_router_cmd_type_e cmd_type);

typedef enum{

    CMD_ROUTER_STRATEGY_ONLINE_FIRST = 0,
    CMD_ROUTER_STRATEGY_OFFLINE_ONLY,
    CMD_ROUTER_STRATEGY_TIMEOUT_FALLBACK
}rcmd_router_strategy_e;


typedef struct{
    rcmd_router_strategy_e strategy;
    uint32_t online_timeout_ms;
    rcmd_router_action_t action;
    rcmd_router_status_action_t status_action;
}rcmd_router_config_t;

/**
 * Initialize command router
 * Called automatically during system initialization
 * @return 0 on success, -1 on failure
 */
int rcmd_router_init(rcmd_router_config_t *config);

/**
 * Process offline voice command
 *
 * This function handles offline voice recognition commands according to the configured strategy:
 * - OFFLINE_ONLY: Execute command immediately
 * - ONLINE_FIRST: Execute only when device is offline
 * - TIMEOUT_FALLBACK: Cache command and wait for online result with timeout
 *
 * @param data - Offline command structure containing command and context
 * @param is_cloud_connected - Whether the device is connected to cloud service
 *
 * @note In TIMEOUT_FALLBACK mode, the command is cached in a list and a delayed task
 *       is submitted to workqueue. If online command arrives before timeout, the
 *       cached command will be invalidated. Otherwise, it will be executed when timeout.
 */
void rcmd_router_offline_cmd_proc(voice_msg_cloud_recognized_command_t *data, bool is_cloud_connected);

/**
 * Process online voice command
 *
 * This function handles online voice recognition commands from cloud service.
 * It will traverse the cached command list and invalidate any matching offline
 * commands to prevent duplicate execution.
 *
 * @param data - Online command structure containing command and context
 *
 * @note This function is ignored in OFFLINE_ONLY strategy mode.
 */
void rcmd_router_online_cmd_proc(voice_msg_cloud_recognized_command_t *data);

/**
 * Process offline status
 *
 * This function handles offline status according to the configured strategy:
 * - OFFLINE_ONLY: Trigger status event immediately with OFFLINE type
 * - Device offline: Trigger status event immediately with OFFLINE type
 * - TIMEOUT_FALLBACK: Currently not implemented (function returns without action)
 *
 * @param status - The status type (e.g., RCMD_ROUTER_STATUS_SESSION_FINISH)
 * @param is_cloud_connected - Whether the device is connected to cloud service
 *
 * @note The status callback will be invoked with the provided status and RCMD_ROUTER_CMD_TYPE_OFFLINE
 */
void rcmd_router_offline_status_proc(rcmd_router_status_e status, bool is_cloud_connected);

/**
 * Process online status
 *
 * This function handles online status from cloud service.
 * It will trigger the status callback with SESSION_FINISH status and ONLINE type.
 *
 * @param status - The status type (currently not used, hardcoded to SESSION_FINISH in implementation)
 *
 * @note This function is ignored in OFFLINE_ONLY strategy mode.
 * @note The status callback is invoked with RCMD_ROUTER_STATUS_SESSION_FINISH (hardcoded) and RCMD_ROUTER_CMD_TYPE_ONLINE
 */
void rcmd_router_online_status_proc(rcmd_router_status_e status);

#ifdef __cplusplus
}
#endif

#endif /* CMD_ROUTER_H */
