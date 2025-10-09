#include <string.h>
#include "FreeRTOS.h"
#include "portable.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "esp_heap_caps_init.h"
#include "utils/lisa_ring_buffer.h"
#include "lisa_mem.h"

#include "lisa_log.h"
#include "lite_dac.h"
#include "mad.h"
#include "cache.h"
#include "low_play.h"
#include "playmp3.h"

#define MP3_RATE (16000) 
#define PLAY_QUE_CNT  (20)
#define PLAY_SAMPLE_BYTE (2)

#define PLAY_EVT_PLAY_START (1<<0)
#define PLAY_EVT_DECODE_COMPLETE (1<<2)
#define PLAY_EVT_PLAY_COMPLETE (1<<1)

#define TAG "PALY_MP3"

#define PLAY_QUE_BUF_SIZE (1152)
typedef struct{
    unsigned char *data;
    int sampe_cnt;
}play_que_t;

static EventGroupHandle_t play_evt_hdl;
#define PLAY_EVT_START          (1 << 0)

static lisa_mp3_cbs_t mp3_cbs;

struct buffer {
    unsigned char data[1024];  // Buffer for file reading
    unsigned long length;
    int is_file_end;
};

static struct 
{
    low_play_t *play;
    uint8_t *buffer;
    struct lisa_ring_buffer *decode_rbuf;
    bool is_play_stop;
}player;

static enum mad_flow input(void *data,
		    struct mad_stream *stream)
{
    struct buffer *buffer = data;
    int bytes_read;

    if (buffer->is_file_end)
        return MAD_FLOW_STOP;

    if (stream->next_frame) {
        int remaining = buffer->length - (stream->next_frame - buffer->data);
        memmove(buffer->data, stream->next_frame, remaining);
        buffer->length = remaining;
    } else {
        buffer->length = 0;
    }

    bytes_read = lisa_ring_buffer_get(player.decode_rbuf, buffer->data + buffer->length, sizeof(buffer->data) - buffer->length);

    if ((bytes_read <= 0) && player.is_play_stop) {
        buffer->is_file_end = 1;
        if (buffer->length == 0)
            return MAD_FLOW_STOP;
    } else {
        buffer->length += bytes_read;
    }

    mad_stream_buffer(stream, buffer->data, buffer->length);
    return MAD_FLOW_CONTINUE;
}

static inline signed int scale(mad_fixed_t sample)
{
    /* round */
    sample += (1L << (MAD_F_FRACBITS - 16));

    /* clip */
    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    /* quantize */
    return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static short mp3buf[PLAY_QUE_BUF_SIZE];
static enum mad_flow output(void *data,
		     struct mad_header const *header,
		     struct mad_pcm *pcm)
{
    int id = 0;
    unsigned int nchannels, nsamples;
    mad_fixed_t const *left_ch, *right_ch;

    /* pcm->samplerate contains the sampling frequency */

    nchannels = pcm->channels;
    nsamples  = pcm->length;
    left_ch   = pcm->samples[0];
    right_ch  = pcm->samples[1];

    // CLOGD("%d-%d", nsamples, id);
    // vTaskDelay(pdMS_TO_TICKS(30));
    while (nsamples--) {
      /* output sample(s) in 16-bit signed little-endian PCM */
      mp3buf[id++] = scale(*left_ch++);
    }
    low_play_pcm_write(low_play_get_handle(), mp3buf, pcm->length);
    return MAD_FLOW_CONTINUE;
}

static enum mad_flow error(void *data,
		    struct mad_stream *stream,
		    struct mad_frame *frame)
{

    LOGD("decoding error 0x%04x (%s) at byte offset \n",
      stream->error, mad_stream_errorstr(stream));

    /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */

    return MAD_FLOW_CONTINUE;
}

static int decode(void)
{
    struct buffer buffer;
    struct mad_decoder decoder;
    int result;

    buffer.length = 0;
    buffer.is_file_end = 0;

    LISA_LOGI(TAG, "init decode");
    /* configure decoder */
    mad_decoder_init(&decoder, &buffer,
          input, 0 /* header */, 0 /* filter */, output,
          error, 0 /* message */);

    /* start decoding */
    LISA_LOGI(TAG, "start decode");
    result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

    /* cleanup */
    mad_decoder_finish(&decoder);
    LISA_LOGI(TAG, "finish decode");

    return result;
}

void lisa_mp3_send(const void *data, uint32_t len)
{
    uint32_t remain = len;
    uint32_t size = 0;
    
    do {

        size = lisa_ring_buffer_put(player.decode_rbuf, (uint8_t *)data, remain);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        remain -= size;
    } while(remain);
}

static void mp3_decode_task(void *param)
{
    uint32_t size = 0;

    while (1) {
        EventBits_t evt_bits = xEventGroupWaitBits(play_evt_hdl, 
            PLAY_EVT_START,
            pdFALSE, pdFALSE, portMAX_DELAY);

        if (evt_bits & PLAY_EVT_START) {
            vTaskDelay(100 / portTICK_PERIOD_MS);

            mp3_cbs.lisa_play_start();
            decode();
            mp3_cbs.lisa_play_stop();
            xEventGroupClearBits(play_evt_hdl, PLAY_EVT_START);
        }
    }

    vTaskDelete(NULL);
}

void mp3_play_init(lisa_mp3_cbs_t *cbs)
{
    if (cbs != NULL) {
        memcpy(&mp3_cbs, cbs, sizeof(lisa_mp3_cbs_t));
    }

    memset(&player, 0, sizeof(player));
    player.buffer = lisa_mem_alloc(16 * 1024);
    if (player.buffer == NULL) {
        LISA_LOGE(TAG, "player.buffer is NULL");
        return;
    }
    player.decode_rbuf = lisa_ring_buffer_init(player.buffer, 16 * 1024);
    if (player.decode_rbuf == NULL) {
        LISA_LOGE(TAG, "player.decode_rbuf is NULL");
        return;
    }

    play_evt_hdl = xEventGroupCreate();
    xTaskCreate(mp3_decode_task, "mp3play", 4 * 1024, NULL, 6, NULL);
}

void mp3_play_start(void)
{
    player.is_play_stop = false;
    xEventGroupSetBits(play_evt_hdl, PLAY_EVT_START);
}

void mp3_play_stop(void)
{
    player.is_play_stop = true;
}
