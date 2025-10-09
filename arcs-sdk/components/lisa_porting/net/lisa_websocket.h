#ifndef __LISA_WEBSOCKET__
#define __LISA_WEBSOCKET__

#include <string.h>
#include <stdint.h>
#include "lisa_err.h"
#include "lisa_queue.h"

// #define SEND_AUDIO_BY_SPEEX
#define SEN_AUDIO_BY_ICO

#define LISA_TEST_PASS (int32_t)(1)
#define LISA_TEST_FAILED (int32_t)(0)
typedef int32_t unit_test_result_t;

typedef enum {
	LISA_WS_OK = 0,
	LISA_WS_COMMON_ERR,
	LISA_WS_DISCONNECT,
} lisa_ws_err_e;

typedef enum {
	LISA_WS_ON_ERROR,
	LISA_WS_ON_HEADER,  // 多次发生
	LISA_WS_ON_CONNECTED,
	LISA_WS_ON_DISCONNECTED,
} lisa_ws_event_e;

typedef struct {
	lisa_ws_event_e what;
	void *user;
} lisa_ws_event_t;

typedef enum {
	LISA_WS_TEXT,
	LISA_WS_BIN,
	LISA_WS_TTS,
} lisa_ws_data_type_e;

typedef struct {
	lisa_ws_data_type_e type;
	const void *buf;
	uint32_t len;
	void *user;
} lisa_ws_data_t;

typedef struct {
	uint8_t *scheme;  // "ws" "wss"
	uint8_t *host;
	uint8_t *path;
	uint8_t *port;
	uint32_t timeout;
    const char * extra_header;
	void *user;
	void (*on_event)(lisa_ws_event_t *event);
	void (*on_data)(lisa_ws_data_t *data);
} lisa_ws_request_t;

typedef struct {
	uint8_t scheme[16];  // "ws" "wss"
	uint8_t host[512];
	uint8_t path[1024];
	uint8_t port[16];
} url_info_t;

typedef struct {
	url_info_t u_info;
	uint32_t timeout;
	bool th_alive;
	bool ws_conn;
	bool ws_stop;
	void *user;
    const char * extra_header;
	lisa_queue_t *tx_msg_queue;
	void (*inter_on_event)(lisa_ws_event_t *event);
	void (*inter_on_data)(lisa_ws_data_t *data);
} lisa_ws_t;

/**
 * @brief init
 * @param  req
 * @return lisa_ws_t*
 */
lisa_ws_t *lisa_ws_init(lisa_ws_request_t *req);

/**
 * @brief connect
 * @param  ins
 * @return lisa_ws_err_e
 */
lisa_ws_err_e lisa_ws_connect(lisa_ws_t *ins);

/**
 * @brief disconnect
 * @param  ins
 * @return lisa_ws_err_e
 */
lisa_ws_err_e lisa_ws_disconnect(lisa_ws_t *ins);

/**
 * @brief send text
 * @param  ins
 * @param  text
 * @return lisa_ws_err_e
 */
lisa_ws_err_e lisa_ws_send_text(lisa_ws_t *ins, const uint8_t *text);

lisa_ws_err_e lisa_ws_send_tts(lisa_ws_t *ins, const uint8_t *text);

/**
 * @brief send binary
 * @param  ins
 * @param  buf
 * @param  len
 * @return lisa_ws_err_e
 */
lisa_ws_err_e lisa_ws_send_binary(lisa_ws_t *ins, const void *buf, uint32_t len);

/**
 * @brief start send binary
 * @param  ins
 * @return lisa_ws_err_e
 */
lisa_ws_err_e lisa_ws_start(lisa_ws_t *ins);

/**
 * @brief stop send binary
 * @param  ins
 * @return lisa_ws_err_e
 */
lisa_ws_err_e lisa_ws_stop(lisa_ws_t *ins);

/**
 * @brief clean up
 * @param  ins
 * @return lisa_ws_err_e
 */
lisa_ws_err_e lisa_ws_cleanup(lisa_ws_t *ins);


lisa_ws_err_e lisa_ws_send_bin(lisa_ws_t *ins, const uint8_t *bin, uint32_t len);

#endif  //__LISA_WEBSOCKET__
