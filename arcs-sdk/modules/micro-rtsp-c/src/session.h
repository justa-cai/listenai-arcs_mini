#ifndef __RTSP__SESSION__H
#define __RTSP__SESSION__H

#include "rtsp.h"

void rtsp_session_init(rtsp_session_t* session, int client, rtsp_streamer_t* streamer);
void rtsp_session_deinit(rtsp_session_t* session);

rtsp_command rtsp_session_handle_request(rtsp_session_t* session, char* request, uint32_t request_size);

bool rtsp_session_start(rtsp_session_t* session, uint32_t read_timeout_ms);

bool rtsp_session_init_transport(rtsp_session_t* session);
void rtsp_session_deinit_transport(rtsp_session_t* session);

#endif // __RTSP__SESSION__H
