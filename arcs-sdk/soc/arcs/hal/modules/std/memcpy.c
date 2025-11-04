/**
  ******************************************************************************
  * @file    memcpy.c
  * @author  ListenAI Application Team
  * @brief   memory copy function implement.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 ListenAI.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ListenAI under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
#include "string.h"
#include <stdint.h>

// =======================================================
// memcpy
// -------------------------------------------------------
/// Copy n bytes from src to dest
/// This implementation access to memory only with read 32 bits and write 32
/// bits. And use a buffer of 2 words for realign the destination
///
/// @param dest Destination buffer
/// @param src Source buffer
/// @param n Copy size
// =======================================================
void *memcpy(void *dest, const void *src, size_t n)
{
    int                     i;
    int                     destOffset;
    int                     srcOffset;
    int                     offset;
    int                     offset0;
    int                     offset1;
    uint32_t                    v;
    uint32_t*                   destAddr;
    uint32_t*                   srcAddr;
    uint32_t                    fifo[2];
    const size_t               addrMask = sizeof(uint32_t)-1;

    destOffset  = (((uint32_t)dest) &  addrMask) * 8;
    srcOffset   = (((uint32_t)src)  &  addrMask) * 8;
    offset      = srcOffset - destOffset;

    destAddr    = (uint32_t*) (((uint32_t)dest) & ~addrMask);
    srcAddr     = (uint32_t*) (((uint32_t)src ) & ~addrMask);

    // No data to copy
    if(n == 0)
    {
        return destAddr;
    }

    // Same alignment on source and destination ?
    if(offset == 0)
    {
        // Destination offset not aligned on word ?
        if(destOffset)
        {
            // Write first bytes

            fifo[0]   = *srcAddr;
            v         = fifo[0];

            for(i = destOffset/8; i < (int32_t)sizeof(uint32_t) && n; ++i, --n)
            {
                ((uint8_t*)destAddr)[i] = (v >> (i*8)) & 0xFF;
            }

            ++srcAddr;
            ++destAddr;
        }

        for(; n >= sizeof(uint32_t); n -= sizeof(uint32_t))
        {
            // Write source on destination with 32 bits access
            fifo[0]   = *srcAddr;
            v         = fifo[0];
            *destAddr = v;

            ++srcAddr;
            ++destAddr;
        }


        if(n != 0)
        {
            // Write last bytes

            fifo[0]   = *srcAddr;
            v         = fifo[0];

            for(i = 0; n; ++i, --n)
            {
                ((uint8_t*)destAddr)[i] = (v >> (i*8)) & 0xFF;
            }

            ++srcAddr;
            ++destAddr;
        }
    }
    else
    {
        if(offset <= 0)
        {
            offset0 = -offset;
            offset1 = (sizeof(uint32_t) * 8) + offset;
        }
        else
        {
            offset1 = offset;
            offset0 = (sizeof(uint32_t) * 8) - offset;
        }

        fifo[1] = *srcAddr;
        ++srcAddr;

        // Destination offset not aligned on word ?
        if(destOffset)
        {
            // Write first bytes
            if(offset > 0)
            {
                fifo[0] = fifo[1];
                fifo[1] = *srcAddr;
                ++srcAddr;
            }

            v         = (fifo[1] << offset0) | (fifo[0] >> offset1);

            for(i = destOffset/8; i < (int32_t)sizeof(uint32_t) && n; ++i, --n)
            {
                ((uint8_t*)destAddr)[i] = (v >> (i*8)) & 0xFF;
            }

            ++destAddr;
        }

        for(i = 0; n >= sizeof(uint32_t); n -= sizeof(uint32_t), i ^= 1)
        {
            // Adjust alignement between source and destination and write in
            // destination buffer with 32 bits access
            fifo[i]   = *srcAddr;
            v         = (fifo[i] << offset0) | (fifo[i^1] >> offset1);
            *destAddr = v;

            ++srcAddr;
            ++destAddr;
        }

        // End of buffer
        if(n != 0)
        {
            // Write last bytes

            fifo[i]   = *srcAddr;
            v         = (fifo[i] << offset0) | (fifo[i^1] >> offset1);

            for(i = 0; n; ++i, --n)
            {
                ((uint8_t*)destAddr)[i] = (v >> (i*8)) & 0xFF;
            }

            ++srcAddr;
            ++destAddr;
        }
    }

    return (void*)dest;
}
