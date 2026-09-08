#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "lisa_thread.h"
#include "lwip/sockets.h"

#define TAG "lisa_player_http"
#include "lisa_log.h"

/* Keep the original ~5.1 s first-byte wait window. */
#define HTTP_RECV_MAX_ATTEMPTS 10U
#define HTTP_RECV_RETRY_DELAY_MS 10U

/* Layout recovered from the prebuilt lisa_player HTTP object's debug info. */
typedef struct {
    int socket_fd;
    uint8_t ssl_context[40];
    int end_retry;
} lisa_player_network_context_t;

_Static_assert(offsetof(lisa_player_network_context_t, end_retry) == 44,
               "Unexpected lisa_player network context layout");

extern void lisaplayer_http_received_hook(int size);

int32_t _recv(lisa_player_network_context_t *network_context,
              void *buffer,
              size_t bytes_to_recv)
{
    int read_len;
    uint32_t attempt = 0;
    int socket_error = 0;

    if (!network_context || !buffer || bytes_to_recv == 0U) {
        LOGE("HTTP recv invalid args: ctx=%p buffer=%p bytes=%u",
             (void *)network_context, buffer, (unsigned)bytes_to_recv);
        return -1;
    }

    do {
        attempt++;
        read_len = recv(network_context->socket_fd, buffer, bytes_to_recv, 0);
        if (read_len >= 0) {
            break;
        }

        socket_error = errno;
        if (socket_error != EAGAIN && socket_error != EINTR) {
            break;
        }

        LOGW("HTTP recv timeout and retry: attempt=%u/%u, errno=%d",
             (unsigned)attempt, (unsigned)HTTP_RECV_MAX_ATTEMPTS, socket_error);
        lisa_thread_mdelay(HTTP_RECV_RETRY_DELAY_MS);
    } while (attempt < HTTP_RECV_MAX_ATTEMPTS && !network_context->end_retry);

    if (read_len > 0) {
        if (attempt > 1U) {
            LOGI("HTTP recv recovered: fd=%d len=%d attempts=%u",
                 network_context->socket_fd, read_len, (unsigned)attempt);
        }
    } else if (read_len == 0) {
        LOGW("HTTP recv EOF: fd=%d attempts=%u",
             network_context->socket_fd, (unsigned)attempt);
    } else {
        LOGE("HTTP recv failed: fd=%d attempts=%u errno=%d end_retry=%d",
             network_context->socket_fd, (unsigned)attempt, socket_error,
             network_context->end_retry);
    }

    lisaplayer_http_received_hook(read_len);
    return read_len;
}
