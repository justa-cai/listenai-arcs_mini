#include <stdint.h>
#include <stdio.h>

#include "../src/rtsp_streamer_timing.h"

static int test_reset_media_timing_clears_video_and_audio_state(void)
{
    rtsp_streamer_t streamer = {0};

    streamer.cseq = 123U;
    streamer.timestamp = 456U;
    streamer.prev_ms = 789U;
    streamer.start_ms = 1000U;
    streamer.audio_timestamp = 2000U;
    streamer.audio_seq_num = 55U;

    rtsp_streamer_reset_media_timing(&streamer);

    if (streamer.cseq != 0U || streamer.timestamp != 0U ||
        streamer.prev_ms != 0U || streamer.start_ms != 0U ||
        streamer.audio_timestamp != 0U || streamer.audio_seq_num != 0U) {
        fprintf(stderr, "expected media timing state to reset to zero\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_reset_media_timing_clears_video_and_audio_state() != 0) {
        return 1;
    }

    printf("rtsp_streamer_timing tests passed\n");
    return 0;
}
