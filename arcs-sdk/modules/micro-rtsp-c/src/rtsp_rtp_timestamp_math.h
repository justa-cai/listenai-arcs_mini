#ifndef RTSP_RTP_TIMESTAMP_MATH_H
#define RTSP_RTP_TIMESTAMP_MATH_H

#include <stdint.h>

uint32_t rtsp_rtp_timestamp_from_elapsed_ms(uint32_t elapsed_ms, uint32_t clock_rate);

#endif /* RTSP_RTP_TIMESTAMP_MATH_H */
