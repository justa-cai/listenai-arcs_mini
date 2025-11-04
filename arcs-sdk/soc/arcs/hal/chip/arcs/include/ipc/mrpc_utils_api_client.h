#ifndef __MRPC_UTILS_API_CLIENT_H__
#define __MRPC_UTILS_API_CLIENT_H__

int ls_read_temp_voltage(int count, float * vout);

int8_t ls_get_mac_from_nvs(uint8_t mac_addr[6]);

int8_t ls_get_mac_customized(uint8_t mac_addr[6]);


#endif