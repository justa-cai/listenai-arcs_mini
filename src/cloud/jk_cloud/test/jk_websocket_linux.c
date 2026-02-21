#define TAG "jk_ws_linux"

#include "jk_websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <fcntl.h>

#define WS_BUFFER_SIZE 4096
#define SHA1_DIGEST_SIZE 20

struct jk_websocket {
    char host[256];
    char port[16];
    char path[256];
    uint32_t timeout_ms;
    void *user;
    jk_ws_event_cb on_event;
    jk_ws_data_cb on_data;
    
    int socket;
    bool connected;
    uint8_t buffer[WS_BUFFER_SIZE];
    char ws_key[64];
    char ws_accept[64];
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

static void generate_ws_key(char *key, size_t size) {
    uint8_t random_bytes[16];
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        fread(random_bytes, 1, 16, f);
        fclose(f);
    } else {
        for (int i = 0; i < 16; i++) {
            random_bytes[i] = rand() & 0xFF;
        }
    }
    base64_encode(random_bytes, 16, key);
}

static int connect_socket(const char *host, const char *port, uint32_t timeout_ms) {
    struct addrinfo hints, *res, *rp;
    int sock = -1;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    printf("[jk_ws] Resolving %s:%s...\n", host, port);
    
    int ret = getaddrinfo(host, port, &hints, &res);
    if (ret != 0) {
        printf("[jk_ws] getaddrinfo failed: %s\n", gai_strerror(ret));
        return -1;
    }
    
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1) continue;
        
        printf("[jk_ws] Socket created, fd=%d\n", sock);
        
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        
        printf("[jk_ws] Connecting to server...\n");
        ret = connect(sock, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0) {
            printf("[jk_ws] Connected immediately\n");
            break;
        }
        
        if (errno == EINPROGRESS) {
            printf("[jk_ws] Connection in progress, waiting...\n");
            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(sock, &write_fds);
            
            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            
            ret = select(sock + 1, NULL, &write_fds, NULL, &tv);
            if (ret > 0) {
                int error = 0;
                socklen_t len = sizeof(error);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);
                if (error == 0) {
                    printf("[jk_ws] Connection successful\n");
                    break;
                }
                printf("[jk_ws] Connection failed: %s\n", strerror(error));
            } else if (ret == 0) {
                printf("[jk_ws] Connection timeout\n");
            } else {
                printf("[jk_ws] select error: %s\n", strerror(errno));
            }
        } else {
            printf("[jk_ws] connect error: %s\n", strerror(errno));
        }
        
        close(sock);
        sock = -1;
    }
    
    freeaddrinfo(res);
    
    if (sock >= 0) {
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
        printf("[jk_ws] Socket set to blocking mode\n");
    }
    
    return sock;
}

static int send_all(int sock, const void *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t ret = send(sock, (const char *)data + sent, len - sent, 0);
        if (ret < 0) {
            if (errno == EINTR) continue;
            printf("[jk_ws] send error: %s\n", strerror(errno));
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
        
        int ret = select(sock + 1, &read_fds, NULL, NULL, &tv);
        if (ret <= 0) {
            printf("[jk_ws] recv_line timeout or error: %d\n", ret);
            return -1;
        }
        
        ssize_t n = recv(sock, buf + i, 1, 0);
        if (n <= 0) {
            printf("[jk_ws] recv error: %s\n", strerror(errno));
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
    char request[1024];
    char line[512];
    
    generate_ws_key(ws->ws_key, sizeof(ws->ws_key));
    
    printf("[jk_ws] Generated WebSocket key: %s\n", ws->ws_key);
    
    int len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        ws->path, ws->host, ws->port, ws->ws_key);
    
    printf("[jk_ws] Sending handshake request:\n%s\n", request);
    
    if (send_all(ws->socket, request, len) != 0) {
        printf("[jk_ws] Failed to send handshake request\n");
        return -1;
    }
    
    printf("[jk_ws] Handshake request sent, waiting for response...\n");
    
    if (recv_line(ws->socket, line, sizeof(line), ws->timeout_ms) <= 0) {
        printf("[jk_ws] Failed to receive HTTP status line\n");
        return -1;
    }
    
    printf("[jk_ws] Received: %s", line);
    
    if (strstr(line, "101") == NULL) {
        printf("[jk_ws] Server did not respond with 101 Switching Protocols\n");
        return -1;
    }
    
    bool got_upgrade = false;
    bool got_connection = false;
    bool got_accept = false;
    
    while (1) {
        if (recv_line(ws->socket, line, sizeof(line), ws->timeout_ms) <= 0) {
            printf("[jk_ws] Failed to receive header line\n");
            return -1;
        }
        
        printf("[jk_ws] Header: %s", line);
        
        if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0) {
            break;
        }
        
        if (strncasecmp(line, "Upgrade:", 8) == 0) {
            got_upgrade = strcasestr(line, "websocket") != NULL;
        } else if (strncasecmp(line, "Connection:", 11) == 0) {
            got_connection = strcasestr(line, "upgrade") != NULL;
        } else if (strncasecmp(line, "Sec-WebSocket-Accept:", 21) == 0) {
            got_accept = true;
            char *value = line + 21;
            while (*value == ' ') value++;
            char *end = strchr(value, '\r');
            if (!end) end = strchr(value, '\n');
            if (end) *end = '\0';
            strncpy(ws->ws_accept, value, sizeof(ws->ws_accept) - 1);
        }
    }
    
    printf("[jk_ws] Handshake result: upgrade=%d, connection=%d, accept=%d\n",
           got_upgrade, got_connection, got_accept);
    
    if (!got_upgrade || !got_connection || !got_accept) {
        printf("[jk_ws] Incomplete handshake response\n");
        return -1;
    }
    
    printf("[jk_ws] WebSocket handshake completed successfully!\n");
    return 0;
}

jk_websocket_t *jk_ws_create(jk_ws_config_t *config) {
    jk_websocket_t *ws = calloc(1, sizeof(jk_websocket_t));
    if (!ws) return NULL;
    
    strncpy(ws->host, config->host ? config->host : "localhost", sizeof(ws->host) - 1);
    strncpy(ws->port, config->port ? config->port : "80", sizeof(ws->port) - 1);
    strncpy(ws->path, config->path ? config->path : "/", sizeof(ws->path) - 1);
    ws->timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 30000;
    ws->user = config->user;
    ws->on_event = config->on_event;
    ws->on_data = config->on_data;
    ws->socket = -1;
    ws->connected = false;
    
    printf("[jk_ws] Created WebSocket client: ws://%s:%s%s\n", ws->host, ws->port, ws->path);
    return ws;
}

void jk_ws_destroy(jk_websocket_t *ws) {
    if (!ws) return;
    
    if (ws->socket >= 0) {
        close(ws->socket);
    }
    free(ws);
}

int jk_ws_connect(jk_websocket_t *ws) {
    if (!ws) return -1;
    
    if (ws->connected) {
        printf("[jk_ws] Already connected\n");
        return 0;
    }
    
    printf("[jk_ws] Connecting to %s:%s%s (timeout=%u ms)\n",
           ws->host, ws->port, ws->path, ws->timeout_ms);
    
    ws->socket = connect_socket(ws->host, ws->port, ws->timeout_ms);
    if (ws->socket < 0) {
        printf("[jk_ws] Failed to connect TCP socket\n");
        if (ws->on_event) {
            ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
        }
        return -1;
    }
    
    printf("[jk_ws] TCP connected, performing WebSocket handshake...\n");
    
    if (perform_handshake(ws) != 0) {
        printf("[jk_ws] Handshake failed\n");
        close(ws->socket);
        ws->socket = -1;
        if (ws->on_event) {
            ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
        }
        return -1;
    }
    
    ws->connected = true;
    
    if (ws->on_event) {
        ws->on_event(ws, JK_WS_EVENT_CONNECTED, ws->user);
    }
    
    printf("[jk_ws] WebSocket connected successfully!\n");
    return 0;
}

int jk_ws_disconnect(jk_websocket_t *ws) {
    if (!ws) return -1;
    
    if (ws->socket >= 0) {
        close(ws->socket);
        ws->socket = -1;
    }
    
    ws->connected = false;
    
    if (ws->on_event) {
        ws->on_event(ws, JK_WS_EVENT_DISCONNECTED, ws->user);
    }
    
    return 0;
}

int jk_ws_send_text(jk_websocket_t *ws, const char *text) {
    if (!ws || !ws->connected || !text) return -1;
    
    size_t len = strlen(text);
    uint8_t frame[10 + len];
    size_t frame_len = 0;
    
    frame[0] = 0x81;
    
    if (len <= 125) {
        frame[1] = len;
        frame_len = 2;
    } else if (len <= 65535) {
        frame[1] = 126;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        frame_len = 4;
    } else {
        frame[1] = 127;
        for (int i = 0; i < 8; i++) {
            frame[2 + i] = (len >> (56 - i * 8)) & 0xFF;
        }
        frame_len = 10;
    }
    
    memcpy(frame + frame_len, text, len);
    frame_len += len;
    
    return send_all(ws->socket, frame, frame_len);
}

int jk_ws_send_binary(jk_websocket_t *ws, const void *data, uint32_t len) {
    if (!ws || !ws->connected || !data) return -1;
    
    uint8_t frame[10 + len];
    size_t frame_len = 0;
    
    frame[0] = 0x82;
    
    if (len <= 125) {
        frame[1] = len;
        frame_len = 2;
    } else if (len <= 65535) {
        frame[1] = 126;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        frame_len = 4;
    } else {
        frame[1] = 127;
        for (int i = 0; i < 8; i++) {
            frame[2 + i] = (len >> (56 - i * 8)) & 0xFF;
        }
        frame_len = 10;
    }
    
    memcpy(frame + frame_len, data, len);
    frame_len += len;
    
    return send_all(ws->socket, frame, frame_len);
}

bool jk_ws_is_connected(jk_websocket_t *ws) {
    return ws && ws->connected;
}

void jk_ws_run_loop(jk_websocket_t *ws, uint32_t timeout_ms) {
    if (!ws || !ws->connected) return;
    
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(ws->socket, &read_fds);
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int ret = select(ws->socket + 1, &read_fds, NULL, NULL, &tv);
    if (ret <= 0) return;
    
    uint8_t header[2];
    ssize_t n = recv(ws->socket, header, 2, 0);
    if (n <= 0) {
        ws->connected = false;
        if (ws->on_event) {
            ws->on_event(ws, JK_WS_EVENT_DISCONNECTED, ws->user);
        }
        return;
    }
    
    uint8_t opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t payload_len = header[1] & 0x7F;
    
    if (payload_len == 126) {
        uint8_t ext[2];
        recv(ws->socket, ext, 2, 0);
        payload_len = (ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        recv(ws->socket, ext, 8, 0);
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }
    
    uint8_t mask[4] = {0};
    if (masked) {
        recv(ws->socket, mask, 4, 0);
    }
    
    if (payload_len > 0 && payload_len < WS_BUFFER_SIZE) {
        recv(ws->socket, ws->buffer, payload_len, 0);
        
        if (masked) {
            for (uint64_t i = 0; i < payload_len; i++) {
                ws->buffer[i] ^= mask[i % 4];
            }
        }
        
        if (ws->on_data && (opcode == 1 || opcode == 2)) {
            jk_ws_data_type_e type = (opcode == 1) ? JK_WS_DATA_TEXT : JK_WS_DATA_BINARY;
            ws->on_data(ws, type, ws->buffer, payload_len, ws->user);
        }
    }
}
