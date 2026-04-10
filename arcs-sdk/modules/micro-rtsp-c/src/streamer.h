#ifndef __RTSP__STREAMER__H
#define __RTSP__STREAMER__H

#include "rtsp.h"

void rtsp_streamer_init(rtsp_streamer_t* streamer, uint16_t width, uint16_t height);
void rtsp_streamer_deinit(rtsp_streamer_t* streamer);

bool rtsp_streamer_start(rtsp_streamer_t* streamer, uint32_t read_timeout_ms);
void rtsp_streamer_add_session(rtsp_streamer_t* streamer, rtsp_session_t* session);
void rtsp_streamer_remove_session(rtsp_streamer_t* streamer, rtsp_session_t* session);
void rtsp_streamer_set_uri(rtsp_streamer_t* streamer, char* host, char* presentation, char* stream);

bool rtsp_streamer_init_udp_transport(rtsp_streamer_t* streamer);
void rtsp_streamer_deinit_udp_transport(rtsp_streamer_t* streamer);

void rtsp_streamer_stream_frame(rtsp_streamer_t* streamer, const uint8_t* data, uint32_t len, uint32_t ms);

// Audio support
void rtsp_streamer_init_audio(rtsp_streamer_t* streamer, const rtsp_audio_config_t* audio_config);
bool rtsp_streamer_init_audio_udp_transport(rtsp_streamer_t* streamer);
void rtsp_streamer_deinit_audio_udp_transport(rtsp_streamer_t* streamer);
void rtsp_streamer_stream_audio_frame(rtsp_streamer_t* streamer, const uint8_t* data, uint32_t len, uint32_t ms);

#endif // __RTSP__STREAMER__H
