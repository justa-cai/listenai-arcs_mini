
#ifndef _LS_UTILS_H_
#define _LS_UTILS_H_

#include <stdint.h>

int ls_read_temp_voltage(float *vout);

int8_t ls_get_mac_from_nvs(uint8_t mac_addr[6]);

int8_t ls_get_mac_customized(uint8_t mac_addr[6]);


#endif
