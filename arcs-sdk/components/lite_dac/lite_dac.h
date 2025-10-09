#ifndef __LITE_DAC__
#define __LITE_DAC__

#include <FreeRTOS.h>
#include <stdint.h>
#include <stdbool.h>

#define ADAC_PA_OPEN      (1)
#define ADAC_PA_CLOSE     (0)

#define ADAC_CTRL_START    (0)
#define ADAC_CTRL_STOP     (1)
#define ADAC_CTRL_VOLUME   (2)
#define ADAC_CTRL_AUD_CFG  (3)
#define ADAC_CTRL_EQ_PARAM     (4)
#define ADAC_CTRL_EQ_LIMITER   (5)

typedef struct
{
    int a_gain;
    int d_gain;
}dac_gain_t;

typedef struct{
    int32_t channel;    //音频声道
    uint32_t rate;      //采样率
    int32_t bit;        //sample的位宽
    dac_gain_t gain;
}dac_aud_t;

int lite_dac_init(void);
int lite_dac_deinit(void);
int lite_dac_left_sample(void);
void dac_pa_ctrl(int enable);
int lite_dac_ctrl(uint32_t uarg, void *parg);
int lite_dac_get_buf(uint8_t **buf, TickType_t xTicksToWait);
int lite_dac_write(void *src, int size, TickType_t xTicksToWait);
bool lite_dac_queue_empty(void);
void lite_dac_pa_pulse_set(int pulse);
int lite_dac_eq_swtich(int sw);
#endif
