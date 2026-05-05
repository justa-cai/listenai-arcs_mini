#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <strings.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "rtsp.h"
#include "rtsp_streamer_timing.h"
#include "streamer.h"
#include "session.h"
#include "session_send_policy.h"
#include "streamer_transport_policy.h"

#define RTSP_WRITE_RESPONSE(format, ...) snprintf(response, sizeof(response), format, __VA_ARGS__)

static char transport[255];
static char sdp_buf[1024];
static char url_buf[1024];

bool rtsp_session_parse_request(rtsp_session_t* session, char* request, uint32_t request_size);
char const* rtsp_session_generate_date_header(void);

static void rtsp_session_send_status(rtsp_session_t* session, const char *status)
{
	static char response[256];

	RTSP_WRITE_RESPONSE(
		"RTSP/1.0 %s\r\nCSeq: %u\r\n"
		"%s\r\n\r\n",
		status,
		session->cseq,
		rtsp_session_generate_date_header()
	);

	rtsp_session_send(session, response, strlen(response));
}

void rtsp_session_handle_command_option(rtsp_session_t* session);
void rtsp_session_handle_command_describe(rtsp_session_t* session);
void rtsp_session_handle_command_setup(rtsp_session_t* session);
void rtsp_session_handle_command_play(rtsp_session_t* session);
void rtsp_session_handle_command_teardown(rtsp_session_t* session);

static int rtsp_socket_send_all(int sock, const void *buf, size_t len) {
	const uint8_t *ptr = (const uint8_t *)buf;
	size_t sent = 0;

	while (sent < len) {
		int ret = send(sock, ptr + sent, len - sent, 0);
		if (ret <= 0) {
			return -1;
		}
		sent += (size_t)ret;
	}

	return 0;
}

static void rtsp_socket_configure_media_client(int sock) {
	int flag = 1;

#ifdef TCP_NODELAY
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) != 0) {
		printf("+ rtsp set TCP_NODELAY failed: sock=%d errno=%d\n", sock, errno);
	}
#endif
}

static void rtsp_socket_set_send_timeout(int sock, uint32_t timeout_ms) {
	struct timeval tv;

	tv.tv_sec = (long)(timeout_ms / 1000U);
	tv.tv_usec = (long)((timeout_ms % 1000U) * 1000U);

	if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
		printf("+ rtsp set send timeout failed: sock=%d timeout=%u errno=%d\n",
		       sock, timeout_ms, errno);
	}
}

int rtsp_session_send(rtsp_session_t* session, const void *buf, size_t len) {
	int ret;
	int saved_errno = 0;
	uint32_t timeout_ms;
	bool is_media_packet;

	if (session == NULL || buf == NULL || len == 0) {
		return -1;
	}

	is_media_packet = rtsp_send_policy_is_interleaved_media_packet(buf, len);
	timeout_ms = rtsp_send_policy_timeout_ms(buf, len);

	if (session->send_lock != NULL) {
		rtos_mutex_lock(session->send_lock);
	}

	rtsp_socket_set_send_timeout(session->client, timeout_ms);
	ret = rtsp_socket_send_all(session->client, buf, len);
	if (ret != 0) {
		saved_errno = errno;
		if (!session->is_stopped) {
			printf("+ rtsp %s send failed: session=%d timeout=%u errno=%d, stopping session\n",
			       is_media_packet ? "media" : "control",
			       session->id, timeout_ms, saved_errno);
			session->is_stopped = true;
		}
		errno = saved_errno;
	}

	if (session->send_lock != NULL) {
		rtos_mutex_unlock(session->send_lock);
	}

	return ret;
}



void rtsp_session_reset_command(rtsp_session_t* session) {
	memset(session->command.presentation, 0x00, sizeof(session->command.presentation));
	memset(session->command.stream, 0x00, sizeof(session->command.stream));
	memset(session->command.host, 0x00, sizeof(session->command.host));
	memset(session->command.track, 0x00, sizeof(session->command.track));
	session->content_length = 0;
}

void rtsp_session_init(rtsp_session_t* session, int client, rtsp_streamer_t* streamer) {
	rtsp_session_reset_command(session);

	session->client = client;
	session->streamer = streamer;
	session->id = rand() | 0x8000000;
	session->stream_id = -1;
	session->rtp_port = 0;
	session->rtcp_port = 0;
	session->video_rtp_port = 0;
	session->video_rtcp_port = 0;
	session->audio_rtp_port = 0;
	session->audio_rtcp_port = 0;
	session->is_tcp_transport = false;
	session->is_streaming = false;
	session->is_stopped = false;
	session->cseq = 0;
	session->command.type = RTSP_UNKNOWN;
	session->is_debug = false;
	session->peer_addr = 0;
	session->send_lock = NULL;
	if (rtos_mutex_create(&session->send_lock) != 0) {
		session->send_lock = NULL;
	}
	rtsp_socket_configure_media_client(client);
}

void rtsp_session_deinit(rtsp_session_t* session) {
	if (rtsp_streamer_close_udp_transport_on_session_deinit()) {
		rtsp_streamer_deinit_udp_transport(session->streamer);
		if (session->streamer->audio_enabled) {
			rtsp_streamer_deinit_audio_udp_transport(session->streamer);
		}
	}
	if (session->send_lock != NULL) {
		rtos_mutex_delete(session->send_lock);
		session->send_lock = NULL;
	}
	close(session->client);
}

bool rtsp_session_init_transport(rtsp_session_t* session){
	if (!session->is_tcp_transport) {
		return rtsp_streamer_init_udp_transport(session->streamer);
	}
	return true;
}

static char* rtsp_session_parse_numeric_header(char* buf, uint32_t *number, uint32_t max_length) {
	int count = max_length;

	// skipping space after ':'
	while (*buf  && count > 0 && (*buf == ' ' || *buf == '\t'))  {
	    ++buf;
	    --count;
	}

	if (!*buf || !isdigit(*buf) || !count)
	    return NULL;

	char *number_start = buf;

	while(*buf && isdigit(*buf) && count > 0) {
	    ++buf;
	    --count;
	}

	if (count == 0)
	    return NULL;

	char c = *buf;

	*buf = '\0';
	*number = atoi(number_start);
	*buf = c;

	return buf;
}

bool rtsp_session_parse_request(rtsp_session_t* session, char* request, uint32_t request_size) {
	static char command[20];

	rtsp_session_reset_command(session);

	char *cursor = request;
	int dest_pos = 0;

	while (dest_pos < 19 && *cursor != ' ' && *cursor != '\t') {
		command[dest_pos++] = *(cursor++);
	}

	command[dest_pos] = '\0';

	while (*cursor && isspace(*cursor))
		++cursor;

	if (!*cursor || 0 != strncasecmp("rtsp://", cursor, 7))
		return false;

	cursor += 7;

	for (dest_pos = 0; *cursor && !isspace(*cursor) && *cursor != '/'; ++cursor, ++dest_pos) {
		if (dest_pos == RTSP_HOSTNAME_MAX_SIZE)
			return false;

		session->command.host[dest_pos] = *cursor;
	}

	if (*cursor != '/')
		return false;

	session->command.host[dest_pos] = '\0';

	if (session->is_debug) printf("host-port: %s\n", session->command.host);

	while (*cursor == '/')
		++cursor;

	for (dest_pos = 0; *cursor && !isspace(*cursor) && *cursor != '/'; ++cursor, ++dest_pos) {
		if (dest_pos == RTSP_PARAM_MAX_SIZE)
			return false;

		session->command.presentation[dest_pos] = *cursor;
	}

	if (*cursor != '/')
		return false;

	session->command.presentation[dest_pos] = '\0';

	if (session->is_debug) printf("+ presentation: %s\n", session->command.presentation);

	while (*cursor == '/')
		++cursor;

	for (dest_pos = 0; *cursor && !isspace(*cursor) && *cursor != '/'; ++cursor, ++dest_pos) {
		if (dest_pos == RTSP_PARAM_MAX_SIZE)
			return false;

		session->command.stream[dest_pos] = *cursor;
	}

	session->command.stream[dest_pos] = '\0';

	if (session->is_debug) printf("+ stream: %s\n", session->command.stream);

	// Parse track component (e.g., /track1, /track2)
	session->command.track[0] = '\0';
	if (*cursor == '/') {
		++cursor;
		for (dest_pos = 0; *cursor && !isspace(*cursor) && *cursor != '/'; ++cursor, ++dest_pos) {
			if (dest_pos == RTSP_PARAM_MAX_SIZE)
				return false;
			session->command.track[dest_pos] = *cursor;
		}
		session->command.track[dest_pos] = '\0';
		if (session->is_debug) printf("+ track: %s\n", session->command.track);
	}

	// Skip any remaining path components
	while (*cursor == '/')
		++cursor;

	// Skip to the next space or end
	while (*cursor && !isspace(*cursor))
		++cursor;

	if (!*cursor || (*cursor != ' ' && *cursor != '\t'))
		return false;

	while (isspace(*cursor))
		++cursor;

	if (0 != strncmp("RTSP/", cursor, 5))
		return false;

	cursor += 5;
	if (!isdigit(*cursor) || cursor[1] != '.' || !isdigit(cursor[2]))
		return false;

	cursor += 3;

	int left;

	if (session->is_debug) printf("analyzing headers\n");

	for (;;) {
		while (*cursor && *cursor != '\r' && cursor[1] != '\n')
			++cursor;

		if (!*cursor || (*cursor != '\r' && cursor[1] != '\n'))
			return false;

		cursor += 2;

		if (!*cursor)
			break;

		left = request_size - (cursor - request);

		if (session->is_debug) {
			printf("* left: %d: '", left);
			for (char *s = cursor; *s && (s - cursor) < 20; ++s) {
				if (*s == '\r')
					printf("<CR>");
				else if (*s == '\n')
					printf("<LF>");
				else if (isprint(*s))
					putchar(*s);
				else
					printf("<0x%x>", *s);
			}

			puts("'");
		}

		if (0 == strncmp("CSeq:", cursor, 5)) {
			uint32_t cseq;

			left -= 5;
			cursor = rtsp_session_parse_numeric_header(cursor + 5, &cseq, left);

			if (cursor == NULL)
				return false;

			session->cseq = cseq;

			if (session->is_debug) printf("+ got cseq: %u\n", cseq);

			continue;
		}

		if (0 == strncmp("Content-Length", cursor, 15)) {
			left -= 15;
			cursor = rtsp_session_parse_numeric_header(cursor + 15, &session->content_length, left);

			if (cursor == NULL)
				return false;

			if (session->is_debug) printf("+ got content-length: %u\n", session->content_length);

			continue;
		}

		for (char *p = cursor; *p; ++p) {
			if (*p == '\r' && p[1] == '\n') {
				if (p[2] != ' ' && p[2] != '\t')
					break;

				*p = ' ';
				++p;
				*p = ' ';
			}
		}

		if (session->command.type == RTSP_SETUP && 0 == strncmp("Transport:", cursor, 10)) {
			cursor += 10;

			while (*cursor && isspace(*cursor))
				++cursor;

			if (0 != strncmp(cursor, "RTP/AVP", 7))
				return false;

			cursor += 7;

			if (0 == strncmp(cursor, "/TCP", 4)) {
				session->is_tcp_transport = true;
				cursor += 4;
			} else {
				session->is_tcp_transport = false;
			}

			if (session->is_debug) printf("+ Transport is %s\n", session->is_tcp_transport ? "TCP" : "UDP");

			session->rtp_port = 0;

			char *next_part, last_char;

			for (;;) {
				while (*cursor == ';' || *cursor == ' ' || *cursor == '\t')
					++cursor;

				if (!*cursor)
					return false;

				if (*cursor == '\r' && cursor[1] == '\n')
					break;

				next_part = strpbrk(cursor, ";\r");

				if (!next_part)
					return false;

				last_char = *next_part;
				*next_part = '\0';

				if (0 == strncmp(cursor, "client_port=", 12)) {
					unsigned int rtp_port = 0;
					unsigned int rtcp_port = 0;
					int ports = sscanf(cursor + 12, "%u-%u", &rtp_port, &rtcp_port);

					if (ports <= 0 || rtp_port == 0U)
						return false;

					session->rtp_port = (uint16_t)rtp_port;
					session->rtcp_port = (ports >= 2 && rtcp_port != 0U) ?
					                     (uint16_t)rtcp_port :
					                     (uint16_t)(session->rtp_port + 1U);

					if (session->is_debug) printf("+ got client port: %u\n", session->rtp_port);
				}

				*next_part = last_char;

				cursor = next_part;
			}
		}

		if (session->is_debug && *cursor != '\r') printf("? unknown header ?\n");

		while (*cursor && *cursor != '\r')
			++cursor;
	}

	printf("\n+ RTSP command: %s\n", command);
	return true;
}

rtsp_command rtsp_session_handle_request(rtsp_session_t* session, char* request, uint32_t request_size) {
	if (rtsp_session_parse_request(session, request, request_size)) {
		switch (session->command.type) {
			case RTSP_OPTIONS:
				rtsp_session_handle_command_option(session);
				break;
			case RTSP_DESCRIBE:
				if (session->is_debug) printf("+ Calling handle_command_describe\n");
				rtsp_session_handle_command_describe(session);
				if (session->is_debug) printf("+ handle_command_describe returned\n");
				break;
			case RTSP_SETUP:
				rtsp_session_handle_command_setup(session);
				break;
			case RTSP_PLAY:
				rtsp_session_handle_command_play(session);
				break;
			case RTSP_TEARDOWN:
				rtsp_session_handle_command_teardown(session);
				break;
			default:
				printf("handle_request: unknown command type: %d\n", session->command.type);
				break;
		}
	} else {
		if (session->is_debug) printf("+ parse_request failed\n");
	}

	return session->command;
}


void rtsp_session_handle_command_option(rtsp_session_t* session) {
	static char response[1024];
	RTSP_WRITE_RESPONSE(
		"RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
		"Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n",
		session->cseq
	);

	rtsp_session_send(session, response, strlen(response));
}

bool rtsp_session_validate_stream_id(rtsp_session_t* session) {
	session->stream_id = -1;
	if (strcmp(session->streamer->presentation, session->command.presentation) == 0 &&
	    strcmp(session->streamer->stream, session->command.stream) == 0) {
		session->stream_id = 0;
	}

	if (session->is_debug) {
		printf("+ validate_stream_id: streamer(%s/%s) vs command(%s/%s) -> stream_id=%d\n",
			session->streamer->presentation, session->streamer->stream,
			session->command.presentation, session->command.stream,
			session->stream_id);
	}

	return session->stream_id == 0;
}

char const* rtsp_session_generate_date_header() {
	static char date_buf[200];
	time_t t = time(NULL);
	strftime(date_buf, sizeof(date_buf), "Date: %a, %b %d %Y %H:%M:%S GMT", gmtime(&t));
	return date_buf;
}

void rtsp_session_handle_command_describe(rtsp_session_t* session) {
	static char response[1024];

	if (session->is_debug) {
		printf("+ handle_command_describe: entered, cseq=%u\n", session->cseq);
	}

	if (!rtsp_session_validate_stream_id(session)) {
		RTSP_WRITE_RESPONSE(
			"RTSP/1.0 404 Stream Not Found\r\nCSeq: %u\r\n%s\r\n",
			session->cseq,
			rtsp_session_generate_date_header()
		);

		rtsp_session_send(session, response, strlen(response));
		return;
	}

	static char buf[256];
	char * clnptr;

	strcpy(buf, session->command.host);
	clnptr = strstr(buf, ":");
	if (clnptr != NULL) clnptr[0] = 0x00;

	int sdp_offset = 0;

	// SDP header
	sdp_offset += snprintf(sdp_buf + sdp_offset, sizeof(sdp_buf) - sdp_offset,
	  "v=0\r\n"
	  "o=- %d 1 IN IP4 %s\r\n"
	  "s=%s\r\n"
	  "i=%s\r\n"
	  "t=0 0\r\n"
	  "a=tool:micro-rtsp-c\r\n"
	  "a=type:broadcast\r\n"
	  "a=control:*\r\n"
	  "a=range:npt=0-\r\n",
	  rand(),
	  buf,
	  session->streamer->audio_enabled ? "AV Stream" : "MJPEG Stream",
	  session->streamer->audio_enabled ? "Audio and Video Stream" : "MJPEG/RTP"
	);

	// Video media description
	sdp_offset += snprintf(sdp_buf + sdp_offset, sizeof(sdp_buf) - sdp_offset,
	  "m=video 0 RTP/AVP 26\r\n"
	  "c=IN IP4 0.0.0.0\r\n"
	  "b=AS:5000\r\n"
	  "a=rtpmap:26 JPEG/90000\r\n"
	  "a=control:track1\r\n"
	  "a=framerate:10\r\n"
	  "a=x-dimensions:%d,%d\r\n",
	  session->streamer->width,
	  session->streamer->height
	);

	// Audio media description (if enabled)
	if (session->streamer->audio_enabled) {
		snprintf(sdp_buf + sdp_offset, sizeof(sdp_buf) - sdp_offset,
		  "m=audio 0 RTP/AVP %u\r\n"
		  "c=IN IP4 0.0.0.0\r\n"
		  "a=rtpmap:%u %s/%u/%u\r\n"
		  "a=control:track2\r\n",
		  session->streamer->audio_config.payload_type,
		  session->streamer->audio_config.payload_type,
		  session->streamer->audio_config.codec_name,
		  session->streamer->audio_config.sample_rate,
		  session->streamer->audio_config.channels
		);
	}

	if (session->is_debug) {
		printf("+ SDP buffer (%d bytes):\n%s\n", (int)strlen(sdp_buf), sdp_buf);
	}

	snprintf(url_buf, sizeof(url_buf),
	  "rtsp://%s/%s/%s",
	  session->command.host,
	  session->command.presentation,
	  session->command.stream
	);

	RTSP_WRITE_RESPONSE(
		"RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
		"%s\r\n"
		"Content-Base: %s/\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: %d\r\n\r\n"
		"%s",
		session->cseq,
		rtsp_session_generate_date_header(),
		url_buf,
		(int)strlen(sdp_buf),
		sdp_buf
	);

	if (session->is_debug) {
		printf("+ DESCRIBE response (%d bytes):\n%s\n", (int)strlen(response), response);
	}

	int sent = rtsp_session_send(session, response, strlen(response)) == 0 ?
	           (int)strlen(response) : -1;

	if (session->is_debug) {
		printf("+ DESCRIBE sent: %d bytes (expected %d)\n", sent, (int)strlen(response));
	}
}

void rtsp_session_handle_command_setup(rtsp_session_t* session) {
	static char response[1024];

	// Check if this is an audio track (track2)
	bool is_audio_track = (strcmp(session->command.track, "track2") == 0);

	// Get peer address for UDP transmission
	struct sockaddr_in peer_addr;
	socklen_t peer_len = sizeof(peer_addr);
	if (getpeername(session->client, (struct sockaddr*)&peer_addr, &peer_len) == 0) {
		session->peer_addr = peer_addr.sin_addr.s_addr;
	}

	// Get local address
	struct sockaddr_in local_addr;
	socklen_t local_len = sizeof(local_addr);
	char peer_ip[32] = "127.0.0.1";
	char local_ip[32] = "127.0.0.1";

	if (getsockname(session->client, (struct sockaddr*)&local_addr, &local_len) == 0) {
		inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, sizeof(local_ip));
	}
	if (getpeername(session->client, (struct sockaddr*)&peer_addr, &peer_len) == 0) {
		inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, sizeof(peer_ip));
	}

	if (is_audio_track && session->streamer->audio_enabled) {
		// Initialize audio transport
		if (!session->is_tcp_transport) {
			if (session->rtp_port == 0 || session->rtcp_port == 0) {
				printf("+ invalid audio UDP transport: missing client_port\n");
				rtsp_session_send_status(session, "461 Unsupported Transport");
				return;
			}
			if (!rtsp_streamer_init_audio_udp_transport(session->streamer)) {
				printf("+ failed to init audio UDP transport\n");
				rtsp_session_send_status(session, "461 Unsupported Transport");
				return;
			}
		}

		// Save audio client ports from Transport header
		session->audio_rtp_port = session->rtp_port;
		session->audio_rtcp_port = session->rtcp_port;

		printf("+ Audio SETUP: client_port=%d-%d server_port=%d-%d\n",
			session->audio_rtp_port, session->audio_rtcp_port,
			session->streamer->audio_rtp_port, session->streamer->audio_rtcp_port);

		// Build response
		if (session->is_tcp_transport) {
			snprintf(transport, sizeof(transport), "RTP/AVP/TCP;unicast;interleaved=2-3");
		} else {
			snprintf(transport, sizeof(transport),
				"RTP/AVP;unicast;destination=%s;source=%s;client_port=%i-%i;server_port=%i-%i",
				peer_ip, local_ip,
				session->audio_rtp_port,
				session->audio_rtcp_port,
				session->streamer->audio_rtp_port,
				session->streamer->audio_rtcp_port
			);
		}
	} else {
		// Video track (track1) - original logic
		if (!session->is_tcp_transport &&
		    (session->rtp_port == 0 || session->rtcp_port == 0)) {
			printf("+ invalid video UDP transport: missing client_port\n");
			rtsp_session_send_status(session, "461 Unsupported Transport");
			return;
		}
		if (!rtsp_session_init_transport(session)) {
			printf("+ failed to init session transport\n");
			rtsp_session_send_status(session, "461 Unsupported Transport");
			return;
		}

		// Save video client ports from Transport header
		session->video_rtp_port = session->rtp_port;
		session->video_rtcp_port = session->rtcp_port;

		printf("+ Video SETUP: client_port=%d-%d server_port=%d-%d\n",
			session->video_rtp_port, session->video_rtcp_port,
			session->streamer->rtp_port, session->streamer->rtcp_port);

		if (session->is_tcp_transport) {
			snprintf(transport, sizeof(transport), "RTP/AVP/TCP;unicast;interleaved=0-1");
		} else {
			snprintf(transport, sizeof(transport),
				"RTP/AVP;unicast;destination=%s;source=%s;client_port=%i-%i;server_port=%i-%i",
				peer_ip, local_ip,
				session->video_rtp_port,
				session->video_rtcp_port,
				session->streamer->rtp_port,
				session->streamer->rtcp_port
			);
		}
	}

	RTSP_WRITE_RESPONSE(
		"RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
		"%s\r\n"
		"Transport: %s\r\n"
		"Session: %i\r\n\r\n",
		session->cseq,
		rtsp_session_generate_date_header(),
		transport,
		session->id
	);

	rtsp_session_send(session, response, strlen(response));
}

void rtsp_session_handle_command_play(rtsp_session_t* session) {
	static char response[1024];
	static char rtp_info[512];
	bool starting_stream;

	starting_stream = !session->is_streaming;

	// Enable streaming
	session->is_streaming = true;
	if (starting_stream) {
		rtsp_streamer_reset_media_timing(session->streamer);
	}

	if (session->is_debug) {
		printf("+ PLAY: enabled streaming (is_streaming=%d, is_tcp=%d, video_port=%d, audio_port=%d)\n",
			session->is_streaming, session->is_tcp_transport,
			session->video_rtp_port, session->audio_rtp_port);
	}

	// Build RTP-Info header
	if (session->streamer->audio_enabled) {
		snprintf(rtp_info, sizeof(rtp_info),
			"RTP-Info: url=rtsp://%s/%s/%s/track1;seq=0;rtptime=0,"
			"url=rtsp://%s/%s/%s/track2;seq=0;rtptime=0",
			session->command.host, session->command.presentation, session->command.stream,
			session->command.host, session->command.presentation, session->command.stream);
	} else {
		snprintf(rtp_info, sizeof(rtp_info),
			"RTP-Info: url=rtsp://%s/%s/%s/track1;seq=0;rtptime=0",
			session->command.host, session->command.presentation, session->command.stream);
	}

	RTSP_WRITE_RESPONSE(
		"RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
		"%s\r\n"
		"Range: npt=0.000-\r\n"
		"Session: %i\r\n"
		"%s\r\n\r\n",
		session->cseq,
		rtsp_session_generate_date_header(),
		session->id,
		rtp_info
	);

	rtsp_session_send(session, response, strlen(response));
}

void rtsp_session_handle_command_teardown(rtsp_session_t* session) {
	static char response[1024];

	RTSP_WRITE_RESPONSE(
		"RTSP/1.0 200 OK\r\nCSeq: %u\r\n"
		"%s\r\n"
		"Session: %i\r\n\r\n",
		session->cseq,
		rtsp_session_generate_date_header(),
		session->id
	);

	rtsp_session_send(session, response, strlen(response));
	printf("+ TEARDOWN received, stopping session\n");
	session->is_stopped = true;
}

static inline int rtsp_client_read_tm(int client, char *buf, size_t size, uint32_t timeout_ms) {
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeout_ms * 1000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    int res = recv(client, buf, size, 0);
    if (res > 0) {
        return res;
    } else if (res == 0) {
        return 0;
    } else {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return -1;
        else
            return 0;
    };
}

bool rtsp_session_start(rtsp_session_t* session, uint32_t read_timeout_ms) {

	if (session->is_stopped) {
		return false;
	}

	static uint32_t buf_pos = 0;
	static enum {
		HDR_STATE_UNKNOWN,
		HDR_STATE_GOT_METHOD,
		HDR_STATE_INVALID
	} state = HDR_STATE_UNKNOWN;

	static char received[RTSP_BUFFER_SIZE];

	if (buf_pos == 0 || buf_pos >= sizeof(received) - 1) {
		memset(received, 0x00, sizeof(received));
		buf_pos = 0;
		state = HDR_STATE_UNKNOWN;
	}

	int res = rtsp_client_read_tm(session->client, received + buf_pos, sizeof(received) - buf_pos - 1, read_timeout_ms);
	if (res > 0) {
		buf_pos += res;
		received[buf_pos] = '\0';

		if (session->is_debug) printf("+ read %d bytes\n", res);
		if (session->is_debug) printf("+ received: \n%s\n", received);

		if (state == HDR_STATE_UNKNOWN && buf_pos >= 6) {
			if (NULL != strstr(received, "\r\n")) {
				char *s = received;

				if (*s == '\r' && *(s + 1) == '\n') s += 2;

				rtsp_session_reset_command(session);

				if (strncmp(s, "OPTIONS ", 8) == 0) session->command.type = RTSP_OPTIONS;
				else if (strncmp(s, "DESCRIBE ", 9) == 0) session->command.type = RTSP_DESCRIBE;
				else if (strncmp(s, "SETUP ", 6) == 0) session->command.type = RTSP_SETUP;
				else if (strncmp(s, "PLAY ", 5) == 0) session->command.type = RTSP_PLAY;
				else if (strncmp(s, "TEARDOWN ", 9) == 0) session->command.type = RTSP_TEARDOWN;

				if (session->command.type != RTSP_UNKNOWN) {
					state = HDR_STATE_GOT_METHOD;
				} else {
					state = HDR_STATE_INVALID;
				}
			}
		}

		if (state != HDR_STATE_UNKNOWN) {
			char *s = strstr(buf_pos > 4 ? received + buf_pos - 4 : received, "\r\n\r\n");

			if (s == NULL) return true;

			if (state == HDR_STATE_INVALID) {
				int len = snprintf(received, sizeof(received), "RTSP/1.0 400 Bad Request\r\nCSeq: %u\r\n\r\n", session->cseq);
				rtsp_session_send(session, received, len);
				buf_pos = 0;
				return false;
			}
		}

		rtsp_command command = rtsp_session_handle_request(session, received, res);

		if (command.type == RTSP_PLAY) {
			session->is_streaming = true;
		} else if (command.type == RTSP_TEARDOWN) {
			session->is_stopped = true;
		}

		state = HDR_STATE_UNKNOWN;
		buf_pos = 0;

		return true;
	} else if (res == 0) {
		printf("+ client closed socket, exiting\n");
		session->is_stopped = true;
		return true;
	} else {
		return false;
	}
}
