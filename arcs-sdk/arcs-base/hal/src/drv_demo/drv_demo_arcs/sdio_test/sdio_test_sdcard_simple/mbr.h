/*
 * mbr.h
 *
 *  Created on: May 10, 2018
 */

#ifndef INCLUDE_MBR_H_
#define INCLUDE_MBR_H_
#include <stdbool.h>
#include <stdint.h>

bool
mbr_get_snum_start(uint8_t* data);

uint32_t
mbr_get_snum_next(uint8_t* data);

#endif /* INCLUDE_MBR_H_ */
