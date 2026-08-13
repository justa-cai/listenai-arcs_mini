
#ifndef _LS_UTILS_H_
#define _LS_UTILS_H_

#include <stdint.h>

/*
 * Memory access macros for register operations
 ****************************************************************************************
 */

#define MEM_RD32(addr)              (*(volatile uint32_t *)(addr))
#define MEM_WR32(addr, value)       (*(volatile uint32_t *)(addr)) = (value)

/*
 * Function declarations
 ****************************************************************************************
 */

int ls_read_temp_voltage(float *vout);

int8_t ls_get_mac_from_nvs(uint8_t mac_addr[6]);

int8_t ls_get_mac_customized(uint8_t mac_addr[6]);
/// @}

#endif
