#ifndef __LUNA_API_MACRO_H__
#define __LUNA_API_MACRO_H__

#include "luna/luna.h"

#define _FAST_FUNC_RO       __attribute__ ((section (".sharedmem.text")))
#define _FAST_DATA_VI       __attribute__ ((section (".sharedmem.data")))       
#define _FAST_DATA_ZI       __attribute__ ((section (".sharedmem.bss")))

#define LUNA_SHARE_ADDR_OFFSET(addr)	((uint32_t *)((uint32_t)(addr)-LUNA_SHARE_MEM_BASE))
#define LUNA_PSRAM_ADDR_OFFSET(addr)	((uint32_t *)((uint32_t)(addr)-LUNA_PSRAM_MEM_BASE))
#define LUNA_FLASH_ADDR_OFFSET(addr)    ((uint32_t *)((uint32_t)(addr)-LUNA_FLASH_MEM_BASE))

#define __luna_cmd_attr__	_FAST_FUNC_RO

unsigned int reg_read(unsigned int addr);
void reg_write(unsigned int addr, unsigned int data);
void luna_print_regs();

#endif	//__LUNA_API_MACRO_H__

