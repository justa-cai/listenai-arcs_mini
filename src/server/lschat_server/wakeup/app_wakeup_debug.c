#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "shell.h"

#define TAG "wakeup_debug"
#include "lisa_log.h"

extern int app_usb_audio_write(void *data, uint32_t sample, uint32_t channel, uint8_t bit);
#if CONFIG_APP_USB_CDC_ENABLE
extern int app_usb_cdc_audio_write(uint8_t *data, uint32_t len);
#endif

#if CONFIG_APP_USB_AUDIO_ENABLE
static volatile bool wakeup_uac_record_flag = true;
#else
static volatile bool wakeup_uac_record_flag = false;
#endif


#define WAKEUP_STREAM_CHANNELS       (5)
#define WAKEUP_STREAM_SAMPLE_CNT     (256)
#define WAKEUP_STREAM_ONE_FRAME_SIZE (WAKEUP_STREAM_SAMPLE_CNT * WAKEUP_STREAM_CHANNELS * sizeof(int16_t))
#define WAKEUP_UAC_CHANNELS          (4)

enum {
    WAKEUP_UAC_CH_MIC = 0,          /* mic */
    WAKEUP_UAC_CH_REF = 1,          /* ref */
    WAKEUP_UAC_CH_ALGO_OUT = 2,     /* algorithm output */
    WAKEUP_UAC_CH_CLOUD_UPLOAD = 3, /* cloud upload */
};


void wakeup_stream_debug_data_output(uint8_t *data, uint32_t len)
{
#if CONFIG_APP_USB_AUDIO_ENABLE
    if (wakeup_uac_record_flag && data && len >= WAKEUP_STREAM_ONE_FRAME_SIZE) {
        uint32_t cnt = len / WAKEUP_STREAM_ONE_FRAME_SIZE;
        const int16_t(*algo_frame)[WAKEUP_STREAM_CHANNELS] = NULL;

        for (uint32_t i = 0; i < cnt; i++) {
            int16_t uac_frame[WAKEUP_STREAM_SAMPLE_CNT][WAKEUP_UAC_CHANNELS];

            algo_frame = (const int16_t(*)[WAKEUP_STREAM_CHANNELS])((const uint8_t *)data +
                                                                    i * WAKEUP_STREAM_ONE_FRAME_SIZE);
            for (uint32_t s = 0; s < WAKEUP_STREAM_SAMPLE_CNT; s++) {
                int16_t mic = algo_frame[s][0];
                int16_t ref = algo_frame[s][2];
                int16_t algo_out = algo_frame[s][3];
                int16_t cloud_upload = algo_frame[s][4];


                uac_frame[s][WAKEUP_UAC_CH_MIC] = mic;
                uac_frame[s][WAKEUP_UAC_CH_REF] = ref;
                uac_frame[s][WAKEUP_UAC_CH_ALGO_OUT] = algo_out;
                uac_frame[s][WAKEUP_UAC_CH_CLOUD_UPLOAD] = cloud_upload;
            }

            app_usb_audio_write(uac_frame, WAKEUP_STREAM_SAMPLE_CNT, WAKEUP_UAC_CHANNELS, 16);
        }
    }
#endif

#if CONFIG_APP_USB_CDC_ENABLE
    // Send to USB CDC if CDC streaming is enabled
    app_usb_cdc_audio_write(data, len);
#endif
}


static int shell_wakeup_uac_record(int argc, char **argv)
{
    if (argc < 2) {
        return -1;
    }

    if (strncmp(argv[1], "on", 2) == 0) {

        if (wakeup_uac_record_flag == false) {
            wakeup_uac_record_flag = true;
            shellPrint(shellGetCurrent(), "wakeup uac record on success\r\n");
        } else {
            shellPrint(shellGetCurrent(), "wakeup uac record already on\r\n");
        }
    } else if (strncmp(argv[1], "off", 3) == 0) {
        if (wakeup_uac_record_flag) {
            wakeup_uac_record_flag = false;
            shellPrint(shellGetCurrent(), "wakeup uac record off success\r\n");
        } else {
            shellPrint(shellGetCurrent(), "wakeup uac record already off\r\n");
        }
    }

    return 0;
}

#if CONFIG_APP_USB_AUDIO_ENABLE
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 wakeup_uac_record, shell_wakeup_uac_record, "control wakeup UAC recording: wakeup uac record on");
#endif
