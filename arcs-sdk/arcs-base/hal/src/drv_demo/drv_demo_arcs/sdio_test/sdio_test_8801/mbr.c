/*
 * mbr.c
 *
 *  Created on: May 9, 2018
 */
#include "mbr.h"

#define MBR_ENTRY_START     0x1BE
#define MBR_NON_FS_DATA     0xDA

static int32_t s_i;

bool
mbr_get_snum_start(uint8_t* data)
{
    s_i = 0;
    if (data[0x1fe] == 0x55 && data[0x1ff] == 0xaa) {
        return true;
    } else {
        return false;
    }
}

uint32_t
mbr_get_snum_next(uint8_t* data)
{
    uint8_t* p = data + MBR_ENTRY_START;
    while (s_i < 4) {
        if (p[s_i * 16 + 4] == MBR_NON_FS_DATA) {
            p = p + s_i * 16 + 8;
            return (*p + (*(p + 1) << 8) + (*(p + 2) << 16) + (*(p + 3) << 24));
        }
        s_i++; //for next loop
    }

    return 0;   //not found
}
