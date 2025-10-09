/*
 * patch.c
 *
 */
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "patch.h"

#define PATCH_MAGIC   0x5450U     /* do not change */
#define FLASH_BASE    0x100000UL

patch_func_ptr_t patch_func_ptr = NULL;
uint32_t patch_header[4] __attribute__((section(".patch_bss"))) ;

// this function must be patched in patch code
static void* patch_func_ptr_default(void *org_func_ptr)
{
	return NULL;
}

static int32_t patch_check(uint16_t data[])
{
	uint16_t xor = 0xff;

	do {
		if(data[0] != PATCH_MAGIC) break;
		if(data[7] != 0) break;
		xor = 0;
		for(int i = 0; i < 6; i++) {
			xor ^= data[i];
		}
		xor = ((xor & 0xff) << 8) + ((xor >> 8) & 0xff);
		xor ^= data[6];
	} while(0);

	return xor ? -1 : 0;
}

void* patch_init(void *param)
{
    // copy flash patch header if needed
    if(patch_check((uint16_t *)patch_header)){
        memcpy(patch_header, (void *)(FLASH_BASE), 16);
    }

	// default ptr
	if(patch_check((uint16_t *)patch_header)) {
		return patch_func_ptr_default;
	}

	return ((patch_func_ptr_t)(patch_header[1]))(param);
}

