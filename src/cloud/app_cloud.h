#ifndef __LISTEN_CLOUD_H__
#define __LISTEN_CLOUD_H__

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	LS_WS_DISCONNECT,
	LS_WS_CONNECT,
	LS_WS_CONNECTING,
} ls_ws_state;

typedef struct app_cloud_s {
	struct lisa_aiui *aiui;
	struct recognizer_s *m_rec;
	struct app_client_s *m_client;
	ls_ws_state ws_state;
	bool m_wifi_conn;
	bool m_fast_reconnect;
	bool m_ntp_conn;
	bool has_notified_network_error;
} app_cloud_t;

app_cloud_t *app_cloud_create(struct app_client_s *app_client);

void app_cloud_process_wifi_connected(app_cloud_t *cloud);

void app_cloud_process_wifi_disconnected(app_cloud_t *cloud);

bool app_cloud_is_connected();

void app_cloud_audio(app_cloud_t *cloud, const char *audio, uint32_t len);

void app_cloud_wakeup(app_cloud_t *cloud);

void app_cloud_tts(const char *text);

void app_cloud_txt(const char *txt);

void app_cloud_ntp_ok(void *arg);

void app_cloud_connect();

void app_cloud_disconnect();

void app_cloud_token_error();
int app_cloud_img_recognition(uint16_t *rgb565_datas, uint32_t width, uint32_t height);

#endif // __LISTEN_CLOUD_H__
