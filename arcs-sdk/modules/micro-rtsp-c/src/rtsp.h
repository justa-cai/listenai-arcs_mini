#ifndef __RTSP__H
#define __RTSP__H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include "rtos_al.h"
#include "linked_list.h"

#define RTSP_BUFFER_SIZE	10000
#define RTSP_PARAM_MAX_SIZE	200
#define RTSP_HOSTNAME_MAX_SIZE	256

typedef struct {
	uint8_t payload_type;
	uint32_t sample_rate;
	uint8_t channels;
	char codec_name[32];
} rtsp_audio_config_t;

typedef struct {
	char *host;
	char *presentation;
	char *stream;

	int rtp_sock;
	int rtcp_sock;

	uint16_t rtp_port;
	uint16_t rtcp_port;

	uint32_t cseq;
	uint32_t timestamp;

	int send_idx;

	llist_item_t* clients;

	uint32_t prev_ms;
	uint32_t start_ms;  // 流开始时间，用于音视频同步
	int udp_rc;

	unsigned short width;
	unsigned short height;

	bool is_debug;

	// Audio support
	int audio_rtp_sock;
	int audio_rtcp_sock;
	uint16_t audio_rtp_port;
	uint16_t audio_rtcp_port;
	uint32_t audio_timestamp;
	uint16_t audio_seq_num;
	int audio_udp_rc;
	rtsp_audio_config_t audio_config;
	bool audio_enabled;
} rtsp_streamer_t;

typedef enum {
    RTSP_OPTIONS,
    RTSP_DESCRIBE,
    RTSP_SETUP,
    RTSP_PLAY,
    RTSP_TEARDOWN,
    RTSP_UNKNOWN
} rtsp_command_type;

typedef struct {
	rtsp_command_type type;
	char presentation[RTSP_PARAM_MAX_SIZE];
	char stream[RTSP_PARAM_MAX_SIZE];
	char host[RTSP_HOSTNAME_MAX_SIZE];
	char track[RTSP_PARAM_MAX_SIZE];
} rtsp_command;

typedef struct {
	int id;
	int client;
	int stream_id;

	uint16_t rtp_port;
	uint16_t rtcp_port;

	rtsp_streamer_t* streamer;
	rtsp_command command;

	uint32_t cseq;
	uint32_t content_length;

	bool is_debug;
	bool is_streaming;
	bool is_stopped;
	bool is_tcp_transport;

	// Video track ports
	uint16_t video_rtp_port;
	uint16_t video_rtcp_port;

	// Audio track ports
	uint16_t audio_rtp_port;
	uint16_t audio_rtcp_port;
	uint32_t peer_addr;
	rtos_mutex send_lock;
} rtsp_session_t;

#endif // __RTSP__H
