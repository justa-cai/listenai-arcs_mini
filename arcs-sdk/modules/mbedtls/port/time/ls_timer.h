#pragma once
#include "mbedtls/platform_time.h"

__attribute__((weak)) mbedtls_time_t ls_time(mbedtls_time_t *timer);

