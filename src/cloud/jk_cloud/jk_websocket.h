#ifndef __JK_WEBSOCKET_H__
#define __JK_WEBSOCKET_H__

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    JK_WS_EVENT_CONNECTED,
    JK_WS_EVENT_DISCONNECTED,
    JK_WS_EVENT_ERROR,
} jk_ws_event_type_e;

typedef enum {
    JK_WS_DATA_TEXT,
    JK_WS_DATA_BINARY,
} jk_ws_data_type_e;

typedef struct jk_websocket jk_websocket_t;

typedef void (*jk_ws_event_cb)(jk_websocket_t *ws, jk_ws_event_type_e event, void *user);
typedef void (*jk_ws_data_cb)(jk_websocket_t *ws, jk_ws_data_type_e type, 
                              const void *data, uint32_t len, void *user);

typedef struct {
    const char *host;
    const char *port;
    const char *path;
    uint32_t timeout_ms;
    void *user;
    jk_ws_event_cb on_event;
    jk_ws_data_cb on_data;
} jk_ws_config_t;

jk_websocket_t *jk_ws_create(jk_ws_config_t *config);
void jk_ws_destroy(jk_websocket_t *ws);

int jk_ws_connect(jk_websocket_t *ws);
int jk_ws_disconnect(jk_websocket_t *ws);

int jk_ws_send_text(jk_websocket_t *ws, const char *text);
int jk_ws_send_binary(jk_websocket_t *ws, const void *data, uint32_t len);

bool jk_ws_is_connected(jk_websocket_t *ws);

void jk_ws_run_loop(jk_websocket_t *ws, uint32_t timeout_ms);

#endif
