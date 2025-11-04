#ifndef __SERIAL_PROTOCOL_H__
#define __SERIAL_PROTOCOL_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TX_FRAME_LEN (32*1024)

uint32_t rfc1662Encode(uint8_t *buf, uint32_t bufLen, uint8_t *pData, uint32_t dataLen);
	
#ifdef __cplusplus
}
#endif

#endif /* __SERIAL_PROTOCOL_H__ */

