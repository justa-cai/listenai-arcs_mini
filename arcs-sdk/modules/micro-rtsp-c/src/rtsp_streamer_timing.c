#include "rtsp_streamer_timing.h"

void rtsp_streamer_reset_media_timing(rtsp_streamer_t* streamer)
{
    if (streamer == NULL) {
        return;
    }

    streamer->cseq = 0;
    streamer->timestamp = 0;
    streamer->prev_ms = 0;
    streamer->start_ms = 0;
    streamer->audio_timestamp = 0;
    streamer->audio_seq_num = 0;
}
