#pragma once

#include <stdint.h>

int cae_uac_init(void);
uint32_t cae_uac_write(const void *data, uint32_t len);
