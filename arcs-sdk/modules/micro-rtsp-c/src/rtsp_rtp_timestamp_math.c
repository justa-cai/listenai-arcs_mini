#include "rtsp_rtp_timestamp_math.h"

uint32_t rtsp_rtp_timestamp_from_elapsed_ms(uint32_t elapsed_ms, uint32_t clock_rate)
{
    uint64_t scaled;

    if (clock_rate == 0U) {
        return 0U;
    }

    scaled = ((uint64_t)elapsed_ms * (uint64_t)clock_rate) / 1000ULL;
    return (uint32_t)scaled;
}
