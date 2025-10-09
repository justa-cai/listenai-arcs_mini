#ifndef __LISA_AIUI_WS_H_
#define __LISA_AIUI_WS_H_

#include "lisa_websocket.h"

typedef struct lisa_aiui_ws {
	// ze_websocket_client_config_t *ws_config;
	lisa_ws_t *ws_client;
} lisa_aiui_ws_t;

#endif