#ifndef __LITE_ADC__
#define __LITE_ADC__

#include <FreeRTOS.h>
#include <stdint.h>

typedef short aud_samp_t;
#ifndef CONFIG_AUDIO_STEP_SAMPS
#define CONFIG_AUDIO_STEP_SAMPS     (256)
#endif
#define CONFIG_AUDIO_STEP_SAMPS_MS  (16)

#define MAPI_AADC_CTRL_REC_START    (0)
#define MAPI_AADC_CTRL_REC_STOP     (1)
#define MAPI_AADC_CTRL_REC_PAUSE    (2)
#define MAPI_AADC_CTRL_REC_RESUME   (3)
#define MAPI_AADC_CTRL_REC_RESET    (4)
#define MAPI_AADC_CTRL_GET_SAMPS    (5)
#define MAPI_AADC_CTRL_SET_GAIN     (6)
#define MAPI_AADC_CTRL_SET_HPF      (7)


int lite_adc_init(void);
int lite_adc_deinit(void);
int lite_adc_available(void);
int lite_adc_ctrl(uint32_t uarg, void *parg);
int lite_adc_read(void *dst, int size, TickType_t msec);
#endif
