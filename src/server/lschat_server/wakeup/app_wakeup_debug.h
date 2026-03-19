#pragma once

#include <stdint.h>

void wakeup_stream_debug_data_input(const void *record_buf, const void *echo_buf, uint32_t record_samples,
                                    uint32_t echo_samples);
void wakeup_stream_debug_data_output(uint8_t *data, uint32_t len);