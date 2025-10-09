#include <string.h>
#include <stdio.h>
#include "serial_protocol.h"

#define RFC1662_FLAG	(0x7E)
#define RFC1662_ESCAPE	(0x7D)

uint32_t rfc1662Encode(uint8_t *buf, uint32_t bufLen, uint8_t *pData, uint32_t dataLen)
{
    uint32_t encLen = 0;

    // Add start flag
    // (bufLen checks at each insert return error on overflow.)
    if (bufLen <= encLen) goto error;
    buf[encLen++] = RFC1662_FLAG;
  
    // Copy pData to buf, escaping flags and escape chars in content
    for (uint32_t n = 0; n < dataLen; n++) {
        // If next byte needs to be escaped...
        if ((pData[n] == RFC1662_FLAG) || (pData[n] == RFC1662_ESCAPE)) {
            // If overflowed, return error
            if (bufLen <= encLen-1) goto error;

            // Add escaped data to buffer
            buf[encLen++] = RFC1662_ESCAPE;
            buf[encLen++] = pData[n] ^ 0x20;
        }
        else {
            // If overflowed, return error
            if (bufLen <= encLen) goto error;

            // Add data to buffer
            buf[encLen++] = pData[n];
        }
    }
    
    // Add stop flag
    if (bufLen <= encLen) goto error;
    buf[encLen++] = RFC1662_FLAG;
    
    // return the length of data in the buffer
    return encLen;
    
    // On error, return 0
error:
    return 0;
}

