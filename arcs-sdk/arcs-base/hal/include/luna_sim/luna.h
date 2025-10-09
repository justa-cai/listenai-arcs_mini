/*
 * luna.h
 *
 *  Created on: Aug 18, 2017
 *      Author: dwwang
 */

#ifndef __LUNA_LUNA_H_SIM__
#define __LUNA_LUNA_H_SIM__

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "luna_error.h"
#include "luna_math_types.h"

#define LUNA_VER_MAJOR 3
#define LUNA_VER_MINOR 0
#define LUNA_VER_PATCH 1
#define LUNA_VER_BUILD 0
#define LUNA_VERSION   ( (LUNA_VER_MAJOR << 24) + (LUNA_VER_MINOR << 16) +  (LUNA_VER_PATCH << 8)  +  (LUNA_VER_BUILD << 0) )

#define LUNA_SHARE_MEM_AHB_BASE			(0x20050000)
#define LUNA_SHARE_MEM_BASE				(0x20050000)
#define LUNA_PSRAM_MEM_BASE             (0x28000000)
#define LUNA_FLASH_MEM_BASE				(0x30000000)

#define LUNA_LOG printf

uint32_t LUNA_API_SIM(luna_version)();

void LUNA_API_SIM(start_counter)();
uint32_t LUNA_API_SIM(get_counter)();

void LUNA_API_SIM(luna_set_check_enable)(int enable);
void LUNA_API_SIM(luna_set_sharedmem)(void* addr, uint32_t size);

#endif /* __LUNA_LUNA_H_SIM__ */
