#pragma once

#include <stdint.h>

int xtts_audio_out_init(void);
int xtts_audio_out_write(const void *data, uint32_t len);
int xtts_audio_out_stop(void);
