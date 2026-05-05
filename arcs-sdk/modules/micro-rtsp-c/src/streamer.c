#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "cache.h"
#include "streamer.h"
#include "linked_list.h"
#include "lisa_thread.h"
#include "rtsp_rtp_timestamp_math.h"
#include "rtsp_streamer_timing.h"
#include "session.h"
#include "sysutils.h"

#define RTSP_VIDEO_SEND_BUF_SIZE 1204
#define RTSP_VIDEO_SEND_BUF_DEPTH 16
#define RTSP_AUDIO_SEND_BUF_SIZE 1200
#define RTSP_AUDIO_SEND_BUF_DEPTH 8

static __psram_bss__ uint8_t s_rtsp_video_send_bufs[RTSP_VIDEO_SEND_BUF_DEPTH][RTSP_VIDEO_SEND_BUF_SIZE];
static __psram_bss__ uint8_t s_rtsp_audio_send_bufs[RTSP_AUDIO_SEND_BUF_DEPTH][RTSP_AUDIO_SEND_BUF_SIZE];
static uint8_t s_rtsp_video_send_buf_idx;
static uint8_t s_rtsp_audio_send_buf_idx;

static inline void rtsp_socker_peer_addr(int __peer_sock, uint32_t *__peer_addr, uint16_t *__peer_port) {
    struct sockaddr_in __peer_r;
    socklen_t __peer_len = sizeof(__peer_r);
    if(getpeername(__peer_sock, (struct sockaddr*)&__peer_r, &__peer_len) < 0) {
        *__peer_addr = 0;
        *__peer_port = 0;
    } else {
        *__peer_port  = __peer_r.sin_port;
        *__peer_addr = __peer_r.sin_addr.s_addr;
    }
}

static inline ssize_t rtsp_udp_socket_send(int __udp_sock, const void *__udp_buf, size_t __udp_len, uint32_t __udp_addr, uint16_t __udp_port)
{
    struct sockaddr_in __udp_addr_in;

    /* UDP TX may hand the payload to non-cache-coherent hardware, so flush the
     * packet bytes before sendto() to avoid stale stack/static contents on air. */
    HAL_FlushDCache_by_Addr((uint32_t *)(uintptr_t)__udp_buf, (uint32_t)__udp_len);

    __udp_addr_in.sin_family      = AF_INET;
    __udp_addr_in.sin_addr.s_addr = __udp_addr;
    __udp_addr_in.sin_port = htons(__udp_port);

    return sendto(__udp_sock, __udp_buf, __udp_len, 0, (struct sockaddr *) &__udp_addr_in, sizeof(__udp_addr_in));
}

static inline int rtsp_udp_socket_create(uint16_t __udp_port)
{
    struct sockaddr_in __udp_addr;

    __udp_addr.sin_family      = AF_INET;
    __udp_addr.sin_addr.s_addr = INADDR_ANY;

    int __udp_s     = socket(AF_INET, SOCK_DGRAM, 0);
    if (__udp_s < 0) {
        printf("+ UDP socket() failed for port %d, errno=%d\n", __udp_port, errno);
        return 0;
    }

    __udp_addr.sin_port = htons(__udp_port);

    if (bind(__udp_s, (struct sockaddr*) &__udp_addr, sizeof(__udp_addr)) != 0) {
        printf("+ UDP bind() failed for port %d, errno=%d\n", __udp_port, errno);
        close(__udp_s);
        __udp_s = 0;
    } else {
        printf("+ UDP socket created successfully: sock=%d port=%d\n", __udp_s, __udp_port);
    }

    return __udp_s;
}

void rtsp_streamer_init(rtsp_streamer_t* streamer, uint16_t width, uint16_t height) {
	streamer->rtp_port = 0;
	streamer->rtcp_port = 0;

	streamer->cseq = 0;
	streamer->timestamp = 0;
	streamer->send_idx = 0;

	streamer->rtp_sock = 0;
	streamer->rtcp_sock = 0;

	streamer->width = width;
	streamer->height = height;

	streamer->clients = NULL;

	streamer->prev_ms = 0;
	streamer->start_ms = 0;  // 将在第一帧时设置
	streamer->udp_rc = 0;

	streamer->is_debug = false;

	streamer->host = "127.0.0.1:554";
	streamer->presentation = "mjpeg";
	streamer->stream = "1";

	// Audio init
	streamer->audio_rtp_sock = 0;
	streamer->audio_rtcp_sock = 0;
	streamer->audio_rtp_port = 0;
	streamer->audio_rtcp_port = 0;
	streamer->audio_timestamp = 0;
	streamer->audio_seq_num = 0;
	streamer->audio_udp_rc = 0;
	streamer->audio_enabled = false;
}

void rtsp_streamer_deinit_session(void* session) {
	rtsp_session_deinit((rtsp_session_t*)session);
}

void rtsp_streamer_deinit(rtsp_streamer_t* streamer) {
	if (streamer->clients != NULL) {
		llist_foreach(streamer->clients, rtsp_streamer_deinit_session);
	}
	llist_item_t* client = streamer->clients;
	while (client != NULL) {
		llist_item_t* next = client->next;
		llist_remove_item(client);
		client = next;
	}
	streamer->clients = NULL;
	rtsp_streamer_deinit_udp_transport(streamer);
	if (streamer->audio_enabled) {
		rtsp_streamer_deinit_audio_udp_transport(streamer);
	}
}

void rtsp_streamer_add_session(rtsp_streamer_t* streamer, rtsp_session_t* session) {
	llist_item_t* item = llist_init((void*)session);
	if (streamer->clients) {
		llist_append(streamer->clients, item);
	} else {
		streamer->clients = item;
	}
}

void rtsp_streamer_remove_session(rtsp_streamer_t* streamer, rtsp_session_t* session) {
	llist_item_t* client = streamer->clients;
	llist_item_t* prev = NULL;

	while (client != NULL) {
		if (client->value == session) {
			// Found the session to remove
			if (prev == NULL) {
				// Removing the first item
				streamer->clients = client->next;
			}
			llist_remove_item(client);
			return;
		}
		prev = client;
		client = client->next;
	}
}

void rtsp_streamer_set_uri(rtsp_streamer_t* streamer, char* host, char* presentation, char* stream) {
	streamer->host = host;
	streamer->presentation = presentation;
	streamer->stream = stream;
}

int rtsp_streamer_send_rtp_packet(rtsp_streamer_t* streamer, const uint8_t* jpeg, int len, int frag_offset, const uint8_t* quant_0_tbl, const uint8_t* quant_1_tbl) {


	#define RTP_HEADER_SIZE 12
	#define JPEG_HEADER_SIZE 8
	#define MAX_FRAG_SIZE 950

	int frag_len = MAX_FRAG_SIZE;

	if (frag_len + frag_offset > len)
		frag_len = len - frag_offset;

	bool is_last_frag = (frag_offset + frag_len) == len;

	if (llist_is_empty(streamer->clients)) {

		return is_last_frag ? 0 : frag_offset;
	}

	// Include quantization tables only in the first fragment for accurate color reproduction
	// Subsequent fragments use the same tables sent in the first fragment
	bool include_quant_tbl = (frag_offset == 0) && (quant_0_tbl != NULL) && (quant_1_tbl != NULL);
	// Q field: Set to 128+ to indicate custom quantization tables are included
	// The actual quality is determined by the quantization tables from the JPEG
	uint8_t q = include_quant_tbl ? 128 : 96;

	uint8_t *buf = s_rtsp_video_send_bufs[s_rtsp_video_send_buf_idx];

	int rtp_packet_size = frag_len + RTP_HEADER_SIZE + JPEG_HEADER_SIZE + (include_quant_tbl ? (4 + (64 * 2)) : 0);
	if ((rtp_packet_size + 4) > RTSP_VIDEO_SEND_BUF_SIZE) {
		printf("+ video RTP packet too large: %d\n", rtp_packet_size + 4);
		return 0;
	}
	s_rtsp_video_send_buf_idx = (uint8_t)((s_rtsp_video_send_buf_idx + 1U) % RTSP_VIDEO_SEND_BUF_DEPTH);


	memset(buf, 0x00, RTSP_VIDEO_SEND_BUF_SIZE);

	buf[0]  = '$';
	buf[1]  = 0;
	buf[2]  = (rtp_packet_size & 0x0000FF00) >> 8;
	buf[3]  = (rtp_packet_size & 0x000000FF);
	buf[4]  = 0x80;
	buf[5]  = 0x1a | (is_last_frag ? 0x80 : 0x00);
	buf[7]  = streamer->cseq & 0x0FF;
	buf[6]  = streamer->cseq >> 8;
	buf[8]  = (streamer->timestamp & 0xFF000000) >> 24;
	buf[9]  = (streamer->timestamp & 0x00FF0000) >> 16;
	buf[10] = (streamer->timestamp & 0x0000FF00) >> 8;
	buf[11] = (streamer->timestamp & 0x000000FF);
	buf[12] = 0x13;
	buf[13] = 0xf9;
	buf[14] = 0x7e;
	buf[15] = 0x67;
	buf[16] = 0x01;  // Type: 1 (4:2:2 decimation)
	buf[17] = (frag_offset & 0x00FF0000) >> 16;
	buf[18] = (frag_offset & 0x0000FF00) >> 8;
	buf[19] = (frag_offset & 0x000000FF);
	buf[20] = 0x00;
	buf[21] = q;
	buf[22] = streamer->width / 8;
	buf[23] = streamer->height / 8;

	int header_len = 24;

	if (include_quant_tbl) {
		buf[24] = 0;
		buf[25] = 0;
		buf[26] = 0;

		int num_quant_bytes = 64;
		buf[27] = 2 * num_quant_bytes;

		header_len += 4;

		memcpy(buf + header_len, quant_0_tbl, num_quant_bytes);
		header_len += num_quant_bytes;

		memcpy(buf + header_len, quant_1_tbl, num_quant_bytes);
		header_len += num_quant_bytes;
	}

	memcpy(buf + header_len, jpeg + frag_offset, frag_len);
	frag_offset += frag_len;

	uint32_t addr;
	uint16_t port;

	llist_item_t* client = streamer->clients;
	rtsp_session_t* session = NULL;
	int sent_count = 0;
		while (client != NULL) {
				session = (rtsp_session_t*)client->value;
				if (session->is_streaming && !session->is_stopped) {
					if (session->is_tcp_transport) {
						int ret = rtsp_session_send(session, buf, rtp_packet_size + 4);
						if (session->is_debug && sent_count == 0) {
							printf("+ TCP send RTP: %d bytes, ret=%d\n", rtp_packet_size + 4, ret);
						}
						if (ret == 0) {
							sent_count++;
						}
				} else {
					rtsp_socker_peer_addr(session->client, &addr, &port);
					int ret = rtsp_udp_socket_send(streamer->rtp_sock, &buf[4], rtp_packet_size, addr, session->video_rtp_port);
					if (session->is_debug && sent_count == 0) {
						printf("+ UDP send VIDEO RTP: sock=%d size=%d addr=%08x port=%d ret=%d\n",
							streamer->rtp_sock, rtp_packet_size, addr, session->video_rtp_port, ret);
					}
						if (ret >= 0) {
							sent_count++;
						}
				}
			} else if (session->is_debug && sent_count == 0) {
				printf("+ skip send: is_streaming=%d, is_stopped=%d\n",
					session->is_streaming, session->is_stopped);
		}

		client = client->next;
	}

	/* Only increment the sequence number when packets are actually sent to clients to avoid increasing it when there are no clients. */
	if (sent_count > 0) {
		streamer->cseq++;
	}

	return is_last_frag ? 0 : frag_offset;
}



bool rtsp_streamer_init_udp_transport(rtsp_streamer_t* streamer) {
	printf("+ init_udp_transport: udp_rc=%d\n", streamer->udp_rc);

	if (streamer->rtp_sock > 0) {
		streamer->udp_rc = 1;
		printf("+ init_udp_transport: already initialized, rtp_sock=%d rtp_port=%d\n",
		       streamer->rtp_sock, streamer->rtp_port);
		return true;
	}

	printf("+ init_udp_transport: trying to create UDP sockets...\n");
	for (uint16_t p = 6970; p < 0xFFFE; p += 2) {
		streamer->rtp_sock = rtsp_udp_socket_create(p);
		if (streamer->rtp_sock) {
			/* We only transmit RTP today, so avoid binding an unused RTCP socket on
			 * the board. That keeps UDP media setup within the limited socket budget. */
			streamer->rtp_port = p;
			streamer->rtcp_port = p + 1;
			streamer->rtcp_sock = 0;
			printf("+ init_udp_transport: SUCCESS - rtp_sock=%d rtp_port=%d rtcp_port=%d (rtcp unbound)\n",
				streamer->rtp_sock, streamer->rtp_port, streamer->rtcp_port);
			break;
		}
	}

	if (streamer->rtp_sock == 0) {
		printf("+ init_udp_transport: FAILED - could not create UDP sockets\n");
		streamer->rtp_port = 0;
		streamer->rtcp_port = 0;
		streamer->rtp_sock = 0;
		streamer->rtcp_sock = 0;
		return false;
	}

	streamer->udp_rc = 1;
	return true;
}
void rtsp_streamer_deinit_udp_transport(rtsp_streamer_t* streamer) {
	if (streamer->rtp_sock <= 0 && streamer->rtcp_sock <= 0) {
		printf("+ deinit_udp_transport: WARNING - not initialized (rc=%d)\n", streamer->udp_rc);
		return;
	}

	streamer->udp_rc = 0;
	streamer->rtp_port = 0;
	streamer->rtcp_port = 0;
	if (streamer->rtp_sock > 0) close(streamer->rtp_sock);
	if (streamer->rtcp_sock > 0) close(streamer->rtcp_sock);
	streamer->rtp_sock = 0;
	streamer->rtcp_sock = 0;
	rtsp_streamer_reset_media_timing(streamer);
	printf("+ deinit_udp_transport: UDP sockets closed, seq/timestamp reset\n");
}

bool rtsp_streamer_start(rtsp_streamer_t* streamer, uint32_t read_timeout_ms) {
	bool ret = true;

	llist_item_t* client = streamer->clients;
	while (client != NULL) {
		rtsp_session_t* session = (rtsp_session_t*)client->value;
		ret &= rtsp_session_start(session, read_timeout_ms);
		client = client->next;
	}

	return ret;
}

bool rtsp_find_jpeg_header(const uint8_t** data, uint32_t *len, uint8_t marker) {
	// NOTE: https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format
	const uint8_t* bytes = *data;

	while (bytes - *data < *len) {
		uint8_t framing = *bytes++;
		if (framing != 0xff) {
			printf("malformed jpeg, framing=%x\n", framing);
			return false;
		}

		uint8_t typecode = *bytes++;
		if (typecode == marker) {
			uint32_t skipped = bytes - *data;


			*data = bytes;
			*len -=skipped;

			return true;
		} else {
			switch (typecode) {
			case 0xd8:
				break;
			case 0xe0:
			case 0xdb:
			case 0xc4:
			case 0xc0:
			case 0xda:
				uint32_t len = bytes[0] * 256 + bytes[1];
				bytes += len;
				break;
			default:
				break;

			}
		}
	}

	printf("failed to find jpeg marker 0x%x\n", marker);

	return false;
}

void rtsp_skip_scan_bytes(const uint8_t** data) {
	const uint8_t* bytes = *data;

	while (true) { // FIXME:
		while (*bytes++ != 0xff);

		if (*bytes++ != 0) {
			*data = bytes - 2;
			return;
		}
	}
}

void rtsp_next_jpeg_block(const uint8_t** data) {
	uint32_t len = (*data)[0] * 256 + (*data)[1];
	*data += len;
}

bool rtsp_decode_jpeg_file(const uint8_t** data, uint32_t* len, const uint8_t** qtable_0, const uint8_t** qtable_1) {
	const uint8_t *bytes = *data;

	if (!rtsp_find_jpeg_header(&bytes, len, 0xd8)) {
		return false;
	}

	*qtable_0 = NULL;
	*qtable_1 = NULL;

	const uint8_t* quant_start = *data;
	uint32_t quant_len = *len;

	if (!rtsp_find_jpeg_header(&quant_start, &quant_len, 0xdb)) {
		printf("failed to find quant table 0\n");
	} else {

		*qtable_0 = quant_start + 3;
		rtsp_next_jpeg_block(&quant_start);

		if (!rtsp_find_jpeg_header(&quant_start, &quant_len, 0xdb)) {
			printf("failed to find quant table 1\n");
		} else {
		}

		*qtable_1 = quant_start + 3;
		rtsp_next_jpeg_block(&quant_start);
	}

	if (!rtsp_find_jpeg_header(data, len, 0xda)) {
		return false;
	}

	uint32_t sos_len = (*data)[0] * 256 + (*data)[1];
	*data += sos_len;
	*len -= sos_len;

	const uint8_t* end_marker = *data;
	uint32_t end_len = *len;

	rtsp_skip_scan_bytes(&end_marker);

	if (!rtsp_find_jpeg_header(&end_marker, &end_len, 0xd9)) {
		return false;
	}

	*len = end_marker - *data;

	return true;
}

void rtsp_streamer_stream_frame(rtsp_streamer_t* streamer, const uint8_t* data, uint32_t len, uint32_t ms) {
	const uint8_t *qtable_0, *qtable_1;
	uint32_t delta_ms;
	uint32_t frame_timestamp;
	uint16_t first_seq;
	uint16_t last_seq;
	int packet_count = 0;
	static uint32_t video_frame_count = 0;

	if (!rtsp_decode_jpeg_file(&data, &len, &qtable_0, &qtable_1)) {
		return;
	}

	/* Compute the RTP timestamp before sending so each JPEG frame carries its
	 * own presentation timestamp instead of reusing the previous frame's. */
	if (streamer->start_ms == 0) {
		streamer->start_ms = ms;
		streamer->timestamp = 0;
		delta_ms = 0;
	} else {
		delta_ms = (ms >= streamer->prev_ms) ? (ms - streamer->prev_ms) : 0;
		streamer->timestamp =
			rtsp_rtp_timestamp_from_elapsed_ms(ms - streamer->start_ms, 90000U);
	}
	streamer->prev_ms = ms;
	frame_timestamp = streamer->timestamp;
	first_seq = (uint16_t)(streamer->cseq & 0xFFFFU);

	// Send all fragments with quantization tables in first packet
	// Add small delay between fragments to prevent UDP packet loss
	int offset = 0;
	do {
		packet_count++;
		offset = rtsp_streamer_send_rtp_packet(streamer, data, len, offset, qtable_0, qtable_1);
	} while (offset != 0);
	last_seq = (uint16_t)((streamer->cseq == 0) ? 0 : ((streamer->cseq - 1U) & 0xFFFFU));

	streamer->send_idx++;
	if (streamer->send_idx > 1) streamer->send_idx = 0;

	video_frame_count++;
	if ((video_frame_count % 100U) == 0U) {
		printf("+ VIDEO RTP: frame=%u jpeg=%u frag=%d ts=%u delta=%u seq=%u-%u qtbl=%d\n",
		       video_frame_count, len, packet_count, frame_timestamp, delta_ms,
		       first_seq, last_seq, (qtable_0 != NULL && qtable_1 != NULL) ? 1 : 0);
	}
}

void rtsp_streamer_init_audio(rtsp_streamer_t* streamer, const rtsp_audio_config_t* audio_config) {
	streamer->audio_config = *audio_config;
	streamer->audio_enabled = true;
	streamer->audio_timestamp = 0;
	streamer->audio_seq_num = 0;
}

bool rtsp_streamer_init_audio_udp_transport(rtsp_streamer_t* streamer) {
	if (streamer->audio_rtp_sock > 0) {
		streamer->audio_udp_rc = 1;
		printf("+ Audio UDP transport already initialized: rtp_port=%d rtcp_port=%d\n",
		       streamer->audio_rtp_port, streamer->audio_rtcp_port);
		return true;
	}

	// Start from a port after video ports to avoid conflicts
	uint16_t start_port = (streamer->rtp_port > 0) ? streamer->rtp_port + 2 : 6972;

	for (uint16_t port = start_port; port < 65534; port += 2) {
		// Skip if this port is already used by video
		if (port == streamer->rtp_port || port == streamer->rtcp_port) {
			continue;
		}

		streamer->audio_rtp_sock = rtsp_udp_socket_create(port);
		if (streamer->audio_rtp_sock) {
			streamer->audio_rtp_port = port;
			streamer->audio_rtcp_port = port + 1;
			streamer->audio_rtcp_sock = 0;
			streamer->audio_udp_rc = 1;
			printf("+ Audio UDP transport: rtp_port=%d rtcp_port=%d (rtcp unbound)\n",
				streamer->audio_rtp_port, streamer->audio_rtcp_port);
			return true;
		}
	}
	streamer->audio_rtp_sock = 0;
	streamer->audio_rtcp_sock = 0;
	streamer->audio_rtp_port = 0;
	streamer->audio_rtcp_port = 0;
	return false;
}

void rtsp_streamer_deinit_audio_udp_transport(rtsp_streamer_t* streamer) {
	if (streamer->audio_rtp_sock <= 0 && streamer->audio_rtcp_sock <= 0) {
		return;
	}
	streamer->audio_udp_rc = 0;
	if (streamer->audio_rtp_sock > 0) close(streamer->audio_rtp_sock);
	if (streamer->audio_rtcp_sock > 0) close(streamer->audio_rtcp_sock);
	streamer->audio_rtp_sock = 0;
	streamer->audio_rtcp_sock = 0;
	streamer->audio_rtp_port = 0;
	streamer->audio_rtcp_port = 0;
	/* Reset audio RTP sequence number and timestamp to avoid sequence issues on next connection */
	streamer->audio_seq_num = 0;
	streamer->audio_timestamp = 0;
	printf("+ deinit_audio_udp_transport: sockets closed, seq/timestamp reset\n");
}

void rtsp_streamer_stream_audio_frame(rtsp_streamer_t* streamer, const uint8_t* data, uint32_t len, uint32_t ms) {
	if (!streamer->audio_enabled || llist_is_empty(streamer->clients)) return;

	uint8_t *rtp_packet = s_rtsp_audio_send_bufs[s_rtsp_audio_send_buf_idx];
	size_t offset = 0;
	uint32_t audio_rate = streamer->audio_config.sample_rate;
	if ((len + 12U) > RTSP_AUDIO_SEND_BUF_SIZE) {
		printf("+ audio RTP packet too large: %u\n", (unsigned int)(len + 12U));
		return;
	}
	s_rtsp_audio_send_buf_idx = (uint8_t)((s_rtsp_audio_send_buf_idx + 1U) % RTSP_AUDIO_SEND_BUF_DEPTH);

	if (audio_rate == 0) {
		audio_rate = 8000;
	}

	rtp_packet[offset++] = 0x80;
	rtp_packet[offset++] = streamer->audio_config.payload_type;
	rtp_packet[offset++] = (streamer->audio_seq_num >> 8) & 0xFF;
	rtp_packet[offset++] = streamer->audio_seq_num & 0xFF;

	// 音视频同步：使用与视频相同的起始时间计算音频时间戳
	// 音频使用 8000 Hz 时钟（采样率）
	if (streamer->start_ms == 0) {
		// 如果视频还没开始，使用当前时间作为起始时间
		streamer->start_ms = ms;
		streamer->audio_timestamp = 0;
	} else {
		// 根据相对于起始时间的偏移计算时间戳
		uint32_t elapsed_ms = ms - streamer->start_ms;
		streamer->audio_timestamp =
			rtsp_rtp_timestamp_from_elapsed_ms(elapsed_ms, audio_rate);
	}

	rtp_packet[offset++] = (streamer->audio_timestamp >> 24) & 0xFF;
	rtp_packet[offset++] = (streamer->audio_timestamp >> 16) & 0xFF;
	rtp_packet[offset++] = (streamer->audio_timestamp >> 8) & 0xFF;
	rtp_packet[offset++] = streamer->audio_timestamp & 0xFF;

	uint32_t ssrc = 0x12345678;
	rtp_packet[offset++] = (ssrc >> 24) & 0xFF;
	rtp_packet[offset++] = (ssrc >> 16) & 0xFF;
	rtp_packet[offset++] = (ssrc >> 8) & 0xFF;
	rtp_packet[offset++] = ssrc & 0xFF;

	memcpy(rtp_packet + offset, data, len);
	offset += len;

	llist_item_t* client = streamer->clients;
	int sent_count = 0;
	while (client != NULL) {
		rtsp_session_t* session = (rtsp_session_t*)client->value;
		// TCP 模式下不检查端口号（端口为0），UDP 模式需要检查端口号
		bool should_send_audio = session->is_streaming &&
		                         !session->is_stopped &&
		                         (session->is_tcp_transport || session->audio_rtp_port > 0);

			if (should_send_audio) {
				if (session->is_tcp_transport) {
					uint8_t tcp_packet[RTSP_AUDIO_SEND_BUF_SIZE + 4];
					int ret;
					tcp_packet[0] = 0x24;
					tcp_packet[1] = 0x02;  // Audio RTP channel
					tcp_packet[2] = (offset >> 8) & 0xFF;
					tcp_packet[3] = offset & 0xFF;
					memcpy(tcp_packet + 4, rtp_packet, offset);
					ret = rtsp_session_send(session, tcp_packet, offset + 4);
					if (session->is_debug && sent_count == 0) {
						printf("+ TCP send AUDIO RTP: %d bytes, channel=2 ret=%d\n", (int)(offset + 4), ret);
					}
					if (ret == 0) {
						sent_count++;
					}
				} else {
					int ret = rtsp_udp_socket_send(streamer->audio_rtp_sock, rtp_packet, offset,
						session->peer_addr, session->audio_rtp_port);
					if (session->is_debug && sent_count == 0) {
					printf("+ UDP send AUDIO RTP: sock=%d size=%d addr=%08x port=%d ret=%d seq=%d ts=%u\n",
							streamer->audio_rtp_sock, (int)offset, session->peer_addr,
							session->audio_rtp_port, ret, streamer->audio_seq_num - 1, streamer->audio_timestamp);
					}
					if (ret >= 0) {
						sent_count++;
					}
				}
			}
		client = client->next;
	}

	/* 只有在实际发送给客户端时才增加音频序列号 */
	if (sent_count > 0) {
		streamer->audio_seq_num++;
	}

	// 每10帧打印一次音频时间戳信息用于调试
	static int audio_frame_count = 0;
	if (++audio_frame_count >= 10) {
		// printf("+ Audio Frame: ms=%u, elapsed=%u, rtp_ts=%u\n",
		// 	ms, ms - streamer->start_ms, streamer->audio_timestamp);
		audio_frame_count = 0;
	}
}
