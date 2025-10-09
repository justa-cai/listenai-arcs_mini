#ifndef __LISA_AUDIO_OUT_H__
#define __LISA_AUDIO_OUT_H__

#include "stdbool.h"

#define AUIDO_OUT_TYPE_LEN (16)
#define AUIDO_OUT_SKILLTYPE_LEN (16)
#define AUIDO_OUT_RES_ID_LEN (64)
#define AUIDO_OUT_VUITTSURL_LEN (256)
#define AUIDO_OUT_URL_LEN (512)
#define AUIDO_OUT_MID_LEN (128)
#define AUIDO_OUT_NAME_LEN (32)
#define AUIDO_OUT_ARTIST_LEN (32)
#define AUIDO_OUT_RATE_LEN (64)

#define TTS_ONLINE_THROW_TIME_MS (100)

typedef struct audio_out_s {
	char m_url[AUIDO_OUT_URL_LEN];
	char mid[AUIDO_OUT_MID_LEN];
	char m_name[AUIDO_OUT_NAME_LEN];
	char m_artist[AUIDO_OUT_ARTIST_LEN];
	char m_all_rate[AUIDO_OUT_RATE_LEN];
	int throw_time;
} audio_out_t;

#endif //__LISA_AUDIO_OUT_H__
