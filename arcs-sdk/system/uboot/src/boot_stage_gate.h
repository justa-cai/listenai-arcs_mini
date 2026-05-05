#ifndef __BOOT_STAGE_GATE_H__
#define __BOOT_STAGE_GATE_H__

#include <stdbool.h>
#include <stdint.h>

#include "boot_config.h"

bool boot_stage_gate_should_enter_second_stage(uint32_t *boot_info_raw, const struct boot_config *cfg);

#endif /* __BOOT_STAGE_GATE_H__ */
