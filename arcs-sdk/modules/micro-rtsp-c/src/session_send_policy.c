#include "session_send_policy.h"

bool rtsp_send_policy_is_interleaved_media_packet(const void *buf, size_t len)
{
    const uint8_t *bytes = (const uint8_t *)buf;

    if (bytes == NULL || len < 4U) {
        return false;
    }

    return bytes[0] == (uint8_t)'$';
}

uint32_t rtsp_send_policy_timeout_ms(const void *buf, size_t len)
{
    if (rtsp_send_policy_is_interleaved_media_packet(buf, len)) {
        return RTSP_MEDIA_SEND_TIMEOUT_MS;
    }

    return RTSP_CONTROL_SEND_TIMEOUT_MS;
}
