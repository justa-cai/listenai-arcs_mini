/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LISA_SYS_CHIP_ID_H_
#define LISA_SYS_CHIP_ID_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int sys_boot_core(uint8_t targe_core_id, uint32_t boot_addr);

#ifdef __cplusplus
}
#endif

#endif /* LISA_SYS_CHIP_ID_H_ */
