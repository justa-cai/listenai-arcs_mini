#ifndef __MRPC_NVS_API_CLIENT_H__
#define __MRPC_NVS_API_CLIENT_H__

uint8_t ipc_nvds_get(uint16_t tag, uint8_t * buf, size_t * buf_len);

uint8_t ipc_nvds_put(uint16_t tag, uint8_t * buf, size_t buf_len);

uint8_t ipc_nvds_del(uint16_t tag);


#endif