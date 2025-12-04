#include "listen_volume.h"
#include "lisa_mutex.h"
#include "app_player.h"
#include "lisa_typedef.h"
#include "kv.h"
#include <math.h>
#include "lisa_kv.h"
#include "comm_service.h"

#define TAG "gain"
#include "lisa_log.h"

#define LISTEN_MIC_GAIN_DEFAULT 80

static lisa_mutex_t *s_mutex = NULL;

static int s_gain = LISTEN_MIC_GAIN_DEFAULT;
static bool s_mute = false;

#define LISTEN_ADC_PDM_GAIN_A_DEFAULT      (30)
#define LISTEN_ADC_PDM_GAIN_A_MAX_DB       (36)
#define LISTEN_ADC_PDM_GAIN_A_MIN_DB       (-12)

#define LISTEN_ADC_PDM_GAIN_D_MAX_DB       (42)
#define LISTEN_ADC_PDM_GAIN_D_MIN_DB       (-83)


static int a_gain_percent_to_db(int percent)
{
	float step;
	float db;
	int idb;
	if(percent >= 50){
		step = (float)(LISTEN_ADC_PDM_GAIN_A_MAX_DB - LISTEN_ADC_PDM_GAIN_A_DEFAULT)/50;
		db = LISTEN_ADC_PDM_GAIN_A_DEFAULT + (percent - 50)*step;
	}else{
		step = (float)(LISTEN_ADC_PDM_GAIN_A_MIN_DB - LISTEN_ADC_PDM_GAIN_A_DEFAULT)/50;
		db = LISTEN_ADC_PDM_GAIN_A_DEFAULT - (percent - 50)*step;
	}

	idb = round(db);
	
	return idb;
}

void listen_mic_gain_set(int gain)
{
	int8_t v_db;
        int8_t v_db_aec;
        int8_t aec_gain;
	s_gain = gain;
	v_db = a_gain_percent_to_db(gain);
        aec_gain = (gain > 50) ? 30: (gain > 20) ? (gain - 20) : 1;
        v_db_aec = a_gain_percent_to_db(aec_gain);
        LISA_LOGI(TAG,"[%s]gain precent:%d convert v_db:%d, aec_gain:%d, v_db_aec:%d", __func__, gain, v_db, aec_gain, v_db_aec);
	lis_ivw_gain_set(v_db, v_db_aec, 0, 0);
	lisa_kv_set_int(KV_KEY_USER_MIC_GAIN, s_gain);
}

int listen_mic_gain_get()
{
	return s_gain;
}

void listen_mic_mute(bool is_mute)
{
	s_mute = is_mute;
	if(s_mute){
		lis_ivw_idle();
	}else{
		lis_ivw_run();
	}
}

int listen_mic_mute_get()
{
	return s_mute;
}


void listen_mic_gain_init()
{

	int r = lisa_kv_get_int(KV_KEY_USER_MIC_GAIN, &s_gain);
	if (r != 0) {
		s_gain = LISTEN_MIC_GAIN_DEFAULT;
	} else {
		if (s_gain > 100) {
			s_gain = 100;
		}

		if (s_gain < 0) {
			s_gain = 0;
		}
	}
	LISA_LOGI(TAG,"[%s]s_gain---: %d\r\n", __func__, s_gain);
    // 设置初始化音量
	listen_mic_gain_set(s_gain);
}
