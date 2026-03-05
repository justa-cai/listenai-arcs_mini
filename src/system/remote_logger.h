#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


int remote_log_output_printf(const uint8_t *log, uint32_t len);

#ifdef __cplusplus
}
#endif