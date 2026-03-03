#define TAG "jk_ws"

#include "jk_websocket.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_thread.h"
#include "lisa_time.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "FreeRTOS.h"
#include "task.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/errno.h"

#define WS_BUFFER_SIZE 8192

struct jk_websocket {
    char host[128];
    char port[8];
    char path[128];
    uint32_t timeout_ms;
    void *user;
    jk_ws_event_cb on_event;
    jk_ws_data_cb on_data;
    
    int socket;
    bool connected;
    bool running;
    lisa_thread_t *thread;
    uint8_t buffer[WS_BUFFER_SIZE];
};

static const char base64_table[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const uint8_t *data, size_t len, char *out) {
    size_t i, j = 0;
    for (i = 0; i < len; i += 3) {
        uint32_t octet_a = i < len ? data[i] : 0;
        uint32_t octet_b = i + 1 < len ? data[i + 1] : 0;
        uint32_t octet_c = i + 2 < len ? data[i + 2] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        
        out[j++] = base64_table[(triple >> 18) & 0x3F];
        out[j++] = base64_table[(triple >> 12) & 0x3F];
        out[j++] = (i + 1 < len) ? base64_table[(triple >> 6) & 0x3F] : '=';
        out[j++] = (i + 2 < len) ? base64_table[triple & 0x3F] : '=';
    }
    out[j] = '\0';
}

static void generate_ws_key(char *key) {
    uint8_t random_bytes[16];
    uint32_t tick = lisa_os_get_tick_ms();
    for (int i = 0; i < 16; i++) {
        random_bytes[i] = (tick >> (i % 4) * 8) ^ (i * 0x5A);
    }
    base64_encode(random_bytes, 16, key);
}

static int connect_socket(const char *host, const char *port, uint32_t timeout_ms) {
    struct addrinfo hints, *res;
    int sock = -1;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    LISA_LOGI(TAG, "Resolving %s:%s...", host, port);
    
    int ret = getaddrinfo(host, port, &hints, &res);
    if (ret != 0) {
        LISA_LOGE(TAG, "getaddrinfo failed: %d", ret);
        return -1;
    }
    
    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        LISA_LOGE(TAG, "socket failed: %d", errno);
        freeaddrinfo(res);
        return -1;
    }
    
    LISA_LOGI(TAG, "Socket created: fd=%d", sock);
    
    int flags = lwip_fcntl(sock, F_GETFL, 0);
    lwip_fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    LISA_LOGI(TAG, "Connecting...");
    ret = lwip_connect(sock, res->ai_addr, res->ai_addrlen);
    
    if (ret != 0 && errno != EINPROGRESS) {
        LISA_LOGE(TAG, "connect failed: %d (%s)", errno, strerror(errno));
        lwip_close(sock);
        freeaddrinfo(res);
        return -1;
    }
    
    if (errno == EINPROGRESS) {
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(sock, &write_fds);
        
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        
        LISA_LOGI(TAG, "Waiting for connection (timeout=%u ms)...", timeout_ms);
        ret = lwip_select(sock + 1, NULL, &write_fds, NULL, &tv);
        
        if (ret <= 0) {
            LISA_LOGE(TAG, "Connection timeout or error: ret=%d", ret);
            lwip_close(sock);
            freeaddrinfo(res);
            return -1;
        }
        
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);
        if (error != 0) {
            LISA_LOGE(TAG, "Connection failed: %d (%s)", error, strerror(error));
            lwip_close(sock);
            freeaddrinfo(res);
            return -1;
        }
    }
    
    freeaddrinfo(res);
    
    flags = lwip_fcntl(sock, F_GETFL, 0);
    lwip_fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
    
    LISA_LOGI(TAG, "TCP connected");
    return sock;
}

static int send_all(int sock, const void *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int ret = lwip_send(sock, (const char *)data + sent, len - sent, 0);
        if (ret < 0) {
            if (errno == EINTR) continue;
            LISA_LOGE(TAG, "send error: %d (%s)", errno, strerror(errno));
            return -1;
        }
        sent += ret;
    }
    return 0;
}

static int recv_line(int sock, char *buf, size_t size, uint32_t timeout_ms) {
    size_t i = 0;
    
    while (i < size - 1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        
        int ret = lwip_select(sock + 1, &read_fds, NULL, NULL, &tv);
        if (ret <= 0) {
            LISA_LOGE(TAG, "recv_line timeout: ret=%d", ret);
            return -1;
        }
        
        int n = lwip_recv(sock, buf + i, 1, 0);
        if (n <= 0) {
            LISA_LOGE(TAG, "recv error: %d (%s)", errno, strerror(errno));
            return -1;
        }
        
        if (buf[i] == '\n') {
            buf[i + 1] = '\0';
            return i + 1;
        }
        i++;
    }
    buf[i] = '\0';
    return i;
}

static int perform_handshake(jk_websocket_t *ws) {
    char request[512];
    char line[256];
    char ws_key[64];
    
    generate_ws_key(ws_key);
    
    LISA_LOGI(TAG, "WebSocket key: %s", ws_key);
    
    int len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        ws->path, ws->host, ws->port, ws_key);
    
    LISA_LOGI(TAG, "Sending handshake request...");
    
    if (send_all(ws->socket, request, len) != 0) {
        LISA_LOGE(TAG, "Failed to send handshake");
        return -1;
    }
    
    LISA_LOGI(TAG, "Waiting for response...");
    
    if (recv_line(ws->socket, line, sizeof(line), ws->timeout_ms) <= 0) {
        LISA_LOGE(TAG, "Failed to receive status line");
        return -1;
    }
    
    LISA_LOGI(TAG, "Status: %s", line);
    
    if (strstr(line, "101") == NULL) {
        LISA_LOGE(TAG, "Server did not respond with 101");
        return -1;
    }
    
    bool got_upgrade = false;
    bool got_connection = false;
    bool got_accept = false;
    
    while (1) {
        if (recv_line(ws->socket, line, sizeof(line), ws->timeout_ms) <= 0) {
            LISA_LOGE(TAG, "Failed to receive header");
            return -1;
        }
        
        if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0) {
            break;
        }
        
        if (strncasecmp(line, "Upgrade:", 8) == 0) {
            got_upgrade = strstr(line, "websocket") != NULL;
        } else if (strncasecmp(line, "Connection:", 11) == 0) {
            got_connection = strstr(line, "pgrade") != NULL;
        } else if (strncasecmp(line, "Sec-WebSocket-Accept:", 21) == 0) {
            got_accept = true;
        }
    }

    LISA_LOGI(TAG, "Handshake: upgrade=%d, connection=%d, accept=%d",
              got_upgrade, got_connection, got_accept);

    if (!got_upgrade || !got_connection || !got_accept) {
        LISA_LOGE(TAG, "Incomplete handshake");
        return -1;
    }

    ws->connected = true;
    LISA_LOGI(TAG, "Handshake completed! connected=true");
    return 0;
}

static void ws_thread(void *arg) {
    jk_websocket_t *ws = (jk_websocket_t *)arg;
    
    LISA_LOGI(TAG, "Thread started, waiting for connect...");
    
    while (ws->running && !ws->connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    if (!ws->running) {
        LISA_LOGW(TAG, "Thread stopped while waiting for connection");
        return;
    }
    
    if (!ws->connected) {
        LISA_LOGW(TAG, "Thread exiting: not connected");
        return;
    }
    
    LISA_LOGI(TAG, "Connected, starting main loop...");
    static int loop_count = 0;

    while (ws->running) {
        if (!ws->connected) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(ws->socket, &read_fds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int ret = lwip_select(ws->socket + 1, &read_fds, NULL, NULL, &tv);
        if (ret <= 0) {
            if (loop_count++ < 10) {
                LISA_LOGD(TAG, "select timeout/return: ret=%d", ret);
            }
            continue;
        }

        if (loop_count < 5) {
            LISA_LOGI(TAG, "select returned %d, data available (loop=%d)", ret, loop_count);
        }
        loop_count++;
        
        uint8_t header[2];
        int n = lwip_recv(ws->socket, header, 2, 0);
        if (n <= 0) {
            LISA_LOGW(TAG, "Connection closed (recv header returned %d)", n);
            ws->connected = false;
            if (ws->on_event) {
                ws->on_event(ws, JK_WS_EVENT_DISCONNECTED, ws->user);
            }
            continue;
        }
        
        uint8_t opcode = header[0] & 0x0F;
        bool masked = (header[1] & 0x80) != 0;
        uint64_t payload_len = header[1] & 0x7F;
        bool fin = (header[0] & 0x80) != 0;
        
        LISA_LOGD(TAG, "WS frame: opcode=%d, masked=%d, len=%llu, fin=%d", 
                  opcode, masked, (unsigned long long)payload_len, fin);
        
        if (opcode == 0x08) {
            LISA_LOGW(TAG, "WebSocket CLOSE frame received");
            if (payload_len >= 2) {
                uint8_t close_data[2];
                int r = lwip_recv(ws->socket, close_data, 2, 0);
                if (r == 2) {
                    uint16_t close_code = ((uint16_t)close_data[0] << 8) | close_data[1];
                    LISA_LOGW(TAG, "Close code: %u (0x%04X)", close_code, close_code);

                    if (payload_len > 2) {
                        uint8_t *reason = lisa_mem_calloc(1, payload_len - 1);
                        if (reason) {
                            uint32_t reason_len = payload_len - 2;
                            uint32_t total = 0;
                            while (total < reason_len) {
                                int read_bytes = lwip_recv(ws->socket, reason + total, reason_len - total, 0);
                                if (read_bytes <= 0) break;
                                total += read_bytes;
                            }
                            LISA_LOGW(TAG, "Close reason: %.*s", (int)reason_len, reason);
                            lisa_mem_free(reason);
                        }
                    }
                }
            }
            ws->connected = false;
            if (ws->on_event) {
                ws->on_event(ws, JK_WS_EVENT_DISCONNECTED, ws->user);
            }
            continue;
        }
        
        if (payload_len == 126) {
            uint8_t ext[2];
            int r = lwip_recv(ws->socket, ext, 2, 0);
            if (r <= 0) {
                LISA_LOGW(TAG, "Connection closed while reading extended length (126)");
                ws->connected = false;
                if (ws->on_event) {
                    ws->on_event(ws, JK_WS_EVENT_DISCONNECTED, ws->user);
                }
                continue;
            }
            if (r != 2) {
                LISA_LOGE(TAG, "Failed to read extended length: got %d bytes", r);
                ws->connected = false;
                if (ws->on_event) {
                    ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
                }
                continue;
            }
            payload_len = ((uint64_t)ext[0] << 8) | ext[1];
        } else if (payload_len == 127) {
            uint8_t ext[8];
            int r = lwip_recv(ws->socket, ext, 8, 0);
            if (r <= 0) {
                LISA_LOGW(TAG, "Connection closed while reading extended length (127)");
                ws->connected = false;
                if (ws->on_event) {
                    ws->on_event(ws, JK_WS_EVENT_DISCONNECTED, ws->user);
                }
                continue;
            }
            if (r != 8) {
                LISA_LOGE(TAG, "Failed to read extended length: got %d bytes", r);
                ws->connected = false;
                if (ws->on_event) {
                    ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
                }
                continue;
            }
            payload_len = 0;
            for (int i = 0; i < 8; i++) {
                payload_len = (payload_len << 8) | ext[i];
            }
        }
        
        uint8_t mask[4] = {0};
        if (masked) {
            int r = lwip_recv(ws->socket, mask, 4, 0);
            if (r <= 0) {
                LISA_LOGW(TAG, "Connection closed while reading mask");
                ws->connected = false;
                if (ws->on_event) {
                    ws->on_event(ws, JK_WS_EVENT_DISCONNECTED, ws->user);
                }
                continue;
            }
            if (r != 4) {
                LISA_LOGE(TAG, "Failed to read mask: got %d bytes", r);
                ws->connected = false;
                if (ws->on_event) {
                    ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
                }
                continue;
            }
        }

        // Handle WebSocket PING frames (opcode 0x09) - must respond with PONG
        if (opcode == 0x09) {
            LISA_LOGI(TAG, "WebSocket PING frame received, payload_len=%llu", (unsigned long long)payload_len);

            // Read the ping payload (if any)
            uint8_t *ping_payload = NULL;
            if (payload_len > 0) {
                if (payload_len < WS_BUFFER_SIZE) {
                    ping_payload = ws->buffer;
                    uint64_t total = 0;
                    while (total < payload_len) {
                        int to_read = payload_len - total;
                        if (to_read > 1024) to_read = 1024;
                        int r = lwip_recv(ws->socket, ping_payload + total, to_read, 0);
                        if (r <= 0) {
                            LISA_LOGE(TAG, "Failed to read ping payload: r=%d", r);
                            ws->connected = false;
                            if (ws->on_event) {
                                ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
                            }
                            continue;
                        }
                        total += r;
                    }

                    // Unmask if masked
                    if (masked) {
                        for (uint64_t i = 0; i < payload_len; i++) {
                            ping_payload[i] ^= mask[i % 4];
                        }
                    }
                } else {
                    LISA_LOGW(TAG, "Ping payload too large (%llu bytes), discarding",
                              (unsigned long long)payload_len);
                    // Skip the payload
                    uint64_t total = 0;
                    while (total < payload_len) {
                        int to_read = payload_len - total;
                        if (to_read > 1024) to_read = 1024;
                        int r = lwip_recv(ws->socket, ws->buffer, to_read, 0);
                        if (r <= 0) break;
                        total += r;
                    }
                    payload_len = 0;  // Don't echo back
                }
            }

            // Respond with PONG frame (opcode 0x0A) with the same payload
            // NOTE: Client MUST mask all frames according to WebSocket protocol
            uint8_t pong_header[6];  // 2 bytes header + 4 bytes mask
            pong_header[0] = 0x8A;  // FIN + PONG opcode

            // Generate random mask
            uint8_t pong_mask[4];
            for (int i = 0; i < 4; i++) {
                pong_mask[i] = (uint8_t)(xTaskGetTickCount() + i);
            }

            if (payload_len < 126) {
                pong_header[1] = payload_len | 0x80;  // Set MASK bit
                memcpy(pong_header + 2, pong_mask, 4);
                lwip_send(ws->socket, pong_header, 6, 0);

                if (payload_len > 0 && ping_payload) {
                    // Mask payload in place and send
                    for (uint64_t i = 0; i < payload_len; i++) {
                        ping_payload[i] ^= pong_mask[i % 4];
                    }
                    lwip_send(ws->socket, ping_payload, payload_len, 0);
                    // Restore original payload (for debugging/logging)
                    for (uint64_t i = 0; i < payload_len; i++) {
                        ping_payload[i] ^= pong_mask[i % 4];
                    }
                }
            } else if (payload_len < 65536) {
                pong_header[1] = 126 | 0x80;  // Set MASK bit
                memcpy(pong_header + 2, pong_mask, 4);
                uint8_t ext[2];
                ext[0] = (payload_len >> 8) & 0xFF;
                ext[1] = payload_len & 0xFF;
                lwip_send(ws->socket, pong_header, 6, 0);
                lwip_send(ws->socket, ext, 2, 0);

                if (ping_payload) {
                    // Mask payload in place and send
                    for (uint64_t i = 0; i < payload_len; i++) {
                        ping_payload[i] ^= pong_mask[i % 4];
                    }
                    lwip_send(ws->socket, ping_payload, payload_len, 0);
                    // Restore original payload
                    for (uint64_t i = 0; i < payload_len; i++) {
                        ping_payload[i] ^= pong_mask[i % 4];
                    }
                }
            }
            // Payload >= 65536 not supported for ping/pong

            LISA_LOGI(TAG, "WebSocket PONG frame sent (masked)");
            continue;
        }

        // Handle WebSocket PONG frames (opcode 0x0A) - server's response to our pings
        if (opcode == 0x0A) {
            LISA_LOGI(TAG, "WebSocket PONG frame received, payload_len=%llu", (unsigned long long)payload_len);

            // Read and discard the pong payload
            if (payload_len > 0 && payload_len < WS_BUFFER_SIZE) {
                uint64_t total = 0;
                while (total < payload_len) {
                    int to_read = payload_len - total;
                    if (to_read > 1024) to_read = 1024;
                    int r = lwip_recv(ws->socket, ws->buffer + total, to_read, 0);
                    if (r <= 0) break;
                    total += r;
                }
                // Unmask if needed (though we just discard it)
                if (masked) {
                    for (uint64_t i = 0; i < payload_len; i++) {
                        ws->buffer[i] ^= mask[i % 4];
                    }
                }
            }
            continue;
        }

        if (payload_len == 0) {
            if (ws->on_data && (opcode == 1 || opcode == 2)) {
                jk_ws_data_type_e type = (opcode == 1) ? JK_WS_DATA_TEXT : JK_WS_DATA_BINARY;
                ws->on_data(ws, type, NULL, 0, ws->user);
            }
        } else if (payload_len < WS_BUFFER_SIZE) {
            uint64_t total = 0;
            while (total < payload_len) {
                int to_read = payload_len - total;
                if (to_read > 1024) to_read = 1024;
                int r = lwip_recv(ws->socket, ws->buffer + total, to_read, 0);
                if (r <= 0) {
                    LISA_LOGE(TAG, "Failed to read payload: r=%d, total=%llu, len=%llu",
                              r, (unsigned long long)total, (unsigned long long)payload_len);
                    ws->connected = false;
                    if (ws->on_event) {
                        ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
                    }
                    continue;
                }
                total += r;
            }

            if (masked) {
                for (uint64_t i = 0; i < payload_len; i++) {
                    ws->buffer[i] ^= mask[i % 4];
                }
            }

            if (ws->on_data && (opcode == 1 || opcode == 2)) {
                jk_ws_data_type_e type = (opcode == 1) ? JK_WS_DATA_TEXT : JK_WS_DATA_BINARY;
                ws->on_data(ws, type, ws->buffer, payload_len, ws->user);
            }
        } else {
            // 大帧分段读取处理
            LISA_LOGI(TAG, "Large payload: %llu bytes, processing in chunks (opcode=%d)",
                      (unsigned long long)payload_len, opcode);

            // 对于 TEXT 帧 (opcode=1)，跳过不处理
            if (opcode == 1) {
                LISA_LOGW(TAG, "Skipping large TEXT frame");
                uint64_t total = 0;
                while (total < payload_len) {
                    int to_read = payload_len - total;
                    if (to_read > 1024) to_read = 1024;
                    uint8_t dummy[1024];
                    int r = lwip_recv(ws->socket, dummy, to_read, 0);
                    if (r <= 0) {
                        LISA_LOGE(TAG, "Failed to skip large TEXT frame: r=%d", r);
                        ws->connected = false;
                        if (ws->on_event) {
                            ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
                        }
                        break;
                    }
                    total += r;
                }
                continue;
            }

            // 对于 BINARY 帧 (opcode=2)，分段读取并传递给回调
            if (opcode == 2 && ws->on_data) {
                uint64_t total_read = 0;
                uint64_t chunk_size = 4096;  // 每次读取 4KB
                uint8_t *chunk_buffer = ws->buffer;

                while (total_read < payload_len) {
                    uint64_t to_read = payload_len - total_read;
                    if (to_read > chunk_size) to_read = chunk_size;

                    int r = lwip_recv(ws->socket, chunk_buffer, to_read, 0);
                    if (r <= 0) {
                        LISA_LOGE(TAG, "Failed to read chunk: r=%d", r);
                        ws->connected = false;
                        if (ws->on_event) {
                            ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
                        }
                        break;
                    }

                    // Unmask the chunk
                    if (masked) {
                        for (int i = 0; i < r; i++) {
                            // 注意：mask 的偏移需要根据全局位置计算
                            uint64_t global_offset = total_read + i;
                            chunk_buffer[i] ^= mask[global_offset % 4];
                        }
                    }

                    // 传递每个数据块给回调
                    ws->on_data(ws, JK_WS_DATA_BINARY, chunk_buffer, r, ws->user);
                    total_read += r;
                }

                LISA_LOGD(TAG, "Large binary frame processed: %llu bytes", (unsigned long long)total_read);
                continue;
            }

            // 其他类型的大帧直接跳过
            LISA_LOGW(TAG, "Skipping large frame (opcode=%d, len=%llu)", opcode,
                      (unsigned long long)payload_len);
            uint64_t total = 0;
            while (total < payload_len) {
                int to_read = payload_len - total;
                if (to_read > 1024) to_read = 1024;
                uint8_t dummy[1024];
                int r = lwip_recv(ws->socket, dummy, to_read, 0);
                if (r <= 0) {
                    LISA_LOGE(TAG, "Failed to skip large frame: r=%d", r);
                    ws->connected = false;
                    if (ws->on_event) {
                        ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
                    }
                    break;
                }
                total += r;
            }
            continue;
        }
    }
    
    ws->connected = false;
    if (ws->on_event) {
        ws->on_event(ws, JK_WS_EVENT_DISCONNECTED, ws->user);
    }
    
    if (ws->socket >= 0) {
        lwip_close(ws->socket);
        ws->socket = -1;
    }
    
    LISA_LOGI(TAG, "Thread exited");
}

jk_websocket_t *jk_ws_create(jk_ws_config_t *config) {
    if (!config) return NULL;
    
    jk_websocket_t *ws = lisa_mem_calloc(1, sizeof(jk_websocket_t));
    if (!ws) {
        LISA_LOGE(TAG, "Failed to allocate");
        return NULL;
    }
    
    strncpy(ws->host, config->host ? config->host : "localhost", sizeof(ws->host) - 1);
    strncpy(ws->port, config->port ? config->port : "80", sizeof(ws->port) - 1);
    strncpy(ws->path, config->path ? config->path : "/", sizeof(ws->path) - 1);
    ws->timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 30000;
    ws->user = config->user;
    ws->on_event = config->on_event;
    ws->on_data = config->on_data;
    ws->socket = -1;
    ws->running = false;
    ws->connected = false;
    
    lisa_thread_attr_t attr = {
        .name = "jk_ws",
        .stack_size = 16 * 1024,
        .priority = LISA_OS_PRIORITY_NORMAL,
    };
    
    ws->running = true;
    
    ws->thread = lisa_thread_create(&attr, ws_thread, ws);
    if (!ws->thread) {
        LISA_LOGE(TAG, "Failed to create thread");
        /* Reset running flag before freeing memory */
        ws->running = false;
        lisa_mem_free(ws);
        return NULL;
    }

    LISA_LOGI(TAG, "Created: ws://%s:%s%s", ws->host, ws->port, ws->path);

    return ws;
}

void jk_ws_destroy(jk_websocket_t *ws) {
    if (!ws) return;
    
    ws->running = false;
    
    if (ws->socket >= 0) {
        lwip_close(ws->socket);
        ws->socket = -1;
    }
    
    lisa_mem_free(ws);
}

int jk_ws_connect(jk_websocket_t *ws) {
    LISA_LOGI(TAG, "jk_ws_connect: called, ws=%p", ws);
    if (!ws) {
        LISA_LOGE(TAG, "jk_ws_connect: ws is NULL!");
        return -1;
    }

    if (ws->connected) {
        LISA_LOGI(TAG, "Already connected, disconnecting first");
        if (ws->socket >= 0) {
            lwip_close(ws->socket);
            ws->socket = -1;
        }
        ws->connected = false;
    }

    LISA_LOGI(TAG, "Connecting to %s:%s%s (on_event=%p, user=%p)",
              ws->host, ws->port, ws->path, ws->on_event, ws->user);

    ws->socket = connect_socket(ws->host, ws->port, ws->timeout_ms);
    if (ws->socket < 0) {
        LISA_LOGE(TAG, "TCP connection failed to %s:%s", ws->host, ws->port);
        if (ws->on_event) {
            LISA_LOGI(TAG, "jk_ws_connect: calling ERROR event callback");
            ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
        } else {
            LISA_LOGW(TAG, "jk_ws_connect: on_event callback is NULL!");
        }
        return -1;
    }

    LISA_LOGI(TAG, "TCP connected, socket=%d, starting WebSocket handshake", ws->socket);

    int ret = perform_handshake(ws);
    if (ret != 0) {
        LISA_LOGE(TAG, "Handshake failed (ret=%d)", ret);
        lwip_close(ws->socket);
        ws->socket = -1;
        ws->connected = false;
        /* 不要设置 ws->running = false，让线程继续运行以便重连 */
        if (ws->on_event) {
            LISA_LOGI(TAG, "jk_ws_connect: calling ERROR event callback after handshake failure");
            ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
        }
        return -1;
    }

    LISA_LOGI(TAG, "WebSocket handshake successful!");

    if (!ws->running) {
        ws->running = true;
    }

    if (ws->on_event) {
        LISA_LOGI(TAG, "jk_ws_connect: calling CONNECTED event callback");
        ws->on_event(ws, JK_WS_EVENT_CONNECTED, ws->user);
    } else {
        LISA_LOGW(TAG, "jk_ws_connect: on_event callback is NULL!");
    }

    return 0;
}

int jk_ws_disconnect(jk_websocket_t *ws) {
    if (!ws) return -1;
    
    ws->running = false;
    ws->connected = false;
    
    if (ws->socket >= 0) {
        lwip_close(ws->socket);
        ws->socket = -1;
    }
    
    return 0;
}

int jk_ws_send_text(jk_websocket_t *ws, const char *text) {
    if (!ws || !text || !ws->connected) return -1;
    
    size_t len = strlen(text);
    uint8_t frame[10 + len];
    size_t frame_len = 0;
    
    frame[0] = 0x81;
    
    if (len <= 125) {
        frame[1] = len | 0x80;
        frame_len = 2;
    } else if (len <= 65535) {
        frame[1] = 126 | 0x80;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        frame_len = 4;
    } else {
        return -1;
    }
    
    uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    memcpy(frame + frame_len, mask, 4);
    frame_len += 4;
    
    for (size_t i = 0; i < len; i++) {
        frame[frame_len + i] = text[i] ^ mask[i % 4];
    }
    frame_len += len;
    
    return send_all(ws->socket, frame, frame_len);
}

int jk_ws_send_binary(jk_websocket_t *ws, const void *data, uint32_t len) {
    if (!ws || !data) {
        LISA_LOGE(TAG, "jk_ws_send_binary: invalid params ws=%p data=%p", ws, data);
        return -1;
    }
    if (!ws->connected) {
        LISA_LOGW(TAG, "jk_ws_send_binary: not connected");
        return -1;
    }

    static int send_count = 0;
    if (send_count++ < 3) {
        LISA_LOGI(TAG, "jk_ws_send_binary: sending %u bytes (count=%d)", len, send_count);
    }

    uint8_t frame[10 + len];
    size_t frame_len = 0;

    frame[0] = 0x82;

    if (len <= 125) {
        frame[1] = len | 0x80;
        frame_len = 2;
    } else if (len <= 65535) {
        frame[1] = 126 | 0x80;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        frame_len = 4;
    } else {
        LISA_LOGE(TAG, "jk_ws_send_binary: payload too large: %u", len);
        return -1;
    }

    uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    memcpy(frame + frame_len, mask, 4);
    frame_len += 4;

    const uint8_t *src = (const uint8_t *)data;
    for (uint32_t i = 0; i < len; i++) {
        frame[frame_len + i] = src[i] ^ mask[i % 4];
    }
    frame_len += len;

    int ret = send_all(ws->socket, frame, frame_len);
    if (ret != 0 && send_count <= 4) {
        LISA_LOGE(TAG, "jk_ws_send_binary: send_all failed: %d", ret);
    } else if (send_count <= 4) {
        LISA_LOGI(TAG, "jk_ws_send_binary: sent %u bytes successfully", frame_len);
    }
    return ret;
}

bool jk_ws_is_connected(jk_websocket_t *ws) {
    return ws && ws->connected;
}

void jk_ws_run_loop(jk_websocket_t *ws, uint32_t timeout_ms) {
    (void)ws;
    (void)timeout_ms;
}
