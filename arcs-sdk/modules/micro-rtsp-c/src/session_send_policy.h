#ifndef __RTSP__SESSION_SEND_POLICY__H
#define __RTSP__SESSION_SEND_POLICY__H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTSP_CONTROL_SEND_TIMEOUT_MS 1000U
/* Interleaved RTP/RTCP media is carried over the RTSP TCP socket, so it must
 * tolerate normal TCP backpressure instead of timing out like best-effort UDP. */
#define RTSP_MEDIA_SEND_TIMEOUT_MS 1000U

bool rtsp_send_policy_is_interleaved_media_packet(const void *buf, size_t len);
uint32_t rtsp_send_policy_timeout_ms(const void *buf, size_t len);

#endif /* __RTSP__SESSION_SEND_POLICY__H */
