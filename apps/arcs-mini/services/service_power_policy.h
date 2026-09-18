#ifndef SERVICE_POWER_POLICY_H
#define SERVICE_POWER_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#include "voice_msg_structure.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef voice_msg_power_policy_state_t service_power_policy_state_t;
typedef voice_msg_power_policy_reason_t service_power_policy_reason_t;

int service_power_policy_init(void);

service_power_policy_state_t service_power_policy_get_state(void);

/* The first single click in hibernate only restores the device. */
bool service_power_policy_handle_function_click(void);

/* Suspend or restore the non-UI consumers used by deep hibernate. */
int service_power_policy_set_runtime_suspended(bool suspended);

/* Keep the foreground miniapp out of idle/hibernate, preserving the existing
 * inactive-time, external-power and battery conditions for auto shutdown. */
void service_power_policy_set_miniapp_active(bool active);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_POWER_POLICY_H */
