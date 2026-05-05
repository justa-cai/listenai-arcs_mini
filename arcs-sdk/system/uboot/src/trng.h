#ifndef UBOOT_TRNG_H
#define UBOOT_TRNG_H

#include <stdint.h>

int trng_get(uint8_t *output, uint32_t len, uint32_t *olen);

#endif
