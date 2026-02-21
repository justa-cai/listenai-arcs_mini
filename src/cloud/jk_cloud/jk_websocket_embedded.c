#define TAG "jk_ws_emb"

#include "jk_websocket.h"
#include "lisa_websocket.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include <string.h>
#include <stdio.h>

struct jk_websocket {
    char host[256];
    char port[16];
    char path[256];
    uint32_t timeout_ms;
    void *user;
    jk_ws_event_cb on_event;
    jk_ws_data_cb on_data;
    
    lisa_ws_t *lisa_ws;
    bool connected;
};

static void lisa_ws_event_handler(lisa_ws_event_t *event) {
    jk_websocket_t *ws = (jk_websocket_t *)event->user;
    if (!ws) return;
    
    LISA_LOGI(TAG, "lisa_ws_event: what=%d", event->what);
    
    switch (event->what) {
    case LISA_WS_ON_CONNECTED:
        LISA_LOGI(TAG, "WebSocket CONNECTED");
        ws->connected = true;
        if (ws->on_event) {
            ws->on_event(ws, JK_WS_EVENT_CONNECTED, ws->user);
        }
        break;
    case LISA_WS_ON_DISCONNECTED:
        LISA_LOGI(TAG, "WebSocket DISCONNECTED");
        ws->connected = false;
        if (ws->on_event) {
            ws->on_event(ws, JK_WS_EVENT_DISCONNECTED, ws->user);
        }
        break;
    case LISA_WS_ON_ERROR:
        LISA_LOGE(TAG, "WebSocket ERROR");
        ws->connected = false;
        if (ws->on_event) {
            ws->on_event(ws, JK_WS_EVENT_ERROR, ws->user);
        }
        break;
    default:
        break;
    }
}

static void lisa_ws_data_handler(lisa_ws_data_t *data) {
    jk_websocket_t *ws = (jk_websocket_t *)data->user;
    if (!ws || !ws->on_data) return;
    
    jk_ws_data_type_e type;
    switch (data->type) {
    case LISA_WS_TEXT:
        type = JK_WS_DATA_TEXT;
        break;
    case LISA_WS_BIN:
    case LISA_WS_TTS:
        type = JK_WS_DATA_BINARY;
        break;
    default:
        return;
    }
    
    LISA_LOGI(TAG, "Received data: type=%d, len=%u", type, data->len);
    ws->on_data(ws, type, data->buf, data->len, ws->user);
}

jk_websocket_t *jk_ws_create(jk_ws_config_t *config) {
    if (!config) return NULL;
    
    jk_websocket_t *ws = lisa_mem_calloc(1, sizeof(jk_websocket_t));
    if (!ws) {
        LISA_LOGE(TAG, "Failed to allocate jk_websocket_t");
        return NULL;
    }
    
    strncpy(ws->host, config->host ? config->host : "localhost", sizeof(ws->host) - 1);
    strncpy(ws->port, config->port ? config->port : "80", sizeof(ws->port) - 1);
    strncpy(ws->path, config->path ? config->path : "/", sizeof(ws->path) - 1);
    ws->timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 30000;
    ws->user = config->user;
    ws->on_event = config->on_event;
    ws->on_data = config->on_data;
    
    lisa_ws_request_t req = {
        .scheme = (uint8_t *)"ws",
        .host = (uint8_t *)ws->host,
        .port = (uint8_t *)ws->port,
        .path = (uint8_t *)ws->path,
        .timeout = ws->timeout_ms,
        .user = ws,
        .on_event = lisa_ws_event_handler,
        .on_data = lisa_ws_data_handler,
    };
    
    ws->lisa_ws = lisa_ws_init(&req);
    if (!ws->lisa_ws) {
        LISA_LOGE(TAG, "Failed to init lisa_ws");
        lisa_mem_free(ws);
        return NULL;
    }
    
    LISA_LOGI(TAG, "Created jk_websocket: ws://%s:%s%s", ws->host, ws->port, ws->path);
    return ws;
}

void jk_ws_destroy(jk_websocket_t *ws) {
    if (!ws) return;
    
    if (ws->lisa_ws) {
        lisa_ws_disconnect(ws->lisa_ws);
        lisa_ws_cleanup(ws->lisa_ws);
    }
    lisa_mem_free(ws);
}

int jk_ws_connect(jk_websocket_t *ws) {
    if (!ws || !ws->lisa_ws) return -1;
    
    if (ws->connected) {
        LISA_LOGI(TAG, "Already connected");
        return 0;
    }
    
    LISA_LOGI(TAG, "Connecting to ws://%s:%s%s (timeout=%u ms)",
              ws->host, ws->port, ws->path, ws->timeout_ms);
    
    lisa_ws_err_e err = lisa_ws_connect(ws->lisa_ws);
    
    LISA_LOGI(TAG, "lisa_ws_connect returned: %d", err);
    
    if (err != LISA_WS_OK) {
        LISA_LOGE(TAG, "Failed to start connection: %d", err);
        return -1;
    }
    
    return 0;
}

int jk_ws_disconnect(jk_websocket_t *ws) {
    if (!ws || !ws->lisa_ws) return -1;
    
    lisa_ws_disconnect(ws->lisa_ws);
    ws->connected = false;
    return 0;
}

int jk_ws_send_text(jk_websocket_t *ws, const char *text) {
    if (!ws || !ws->lisa_ws || !text || !ws->connected) return -1;
    return lisa_ws_send_text(ws->lisa_ws, (const uint8_t *)text);
}

int jk_ws_send_binary(jk_websocket_t *ws, const void *data, uint32_t len) {
    if (!ws || !ws->lisa_ws || !data || !ws->connected) return -1;
    return lisa_ws_send_binary(ws->lisa_ws, data, len);
}

bool jk_ws_is_connected(jk_websocket_t *ws) {
    return ws && ws->connected;
}

void jk_ws_run_loop(jk_websocket_t *ws, uint32_t timeout_ms) {
    (void)ws;
    (void)timeout_ms;
}
