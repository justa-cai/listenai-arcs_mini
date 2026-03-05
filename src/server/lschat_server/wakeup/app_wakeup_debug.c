#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "shell.h"
#include "mbedtls/md5.h"

#define TAG "wakeup_debug"
#include "lisa_log.h"

static volatile bool wakeup_uac_record_flag = false;
extern int app_usb_audio_write(void *data, uint32_t sample, uint32_t channel, uint8_t bit);
#if CONFIG_APP_USB_CDC_ENABLE
extern int app_usb_cdc_audio_write(uint8_t *data, uint32_t len);
#endif

// MD5 context for CDC stream
static mbedtls_md5_context cdc_md5_ctx;
static bool cdc_md5_initialized = false;
static uint64_t cdc_total_bytes = 0;


void wakeup_stream_debug_data_output(uint8_t *data, uint32_t len){
    #define WAKEUP_STREAM_CHANNELS          (5)
    #define WAKEUP_STREAM_SAMPLE_CNT        (256)
    #define WAKEUP_STREAM_ONE_FRAME_SIZE    (WAKEUP_STREAM_SAMPLE_CNT * WAKEUP_STREAM_CHANNELS * sizeof(short))

#if CONFIG_APP_USB_AUDIO_ENABLE
    // Send to USB Audio if UAC recording is enabled
    if(wakeup_uac_record_flag){
        int cnt = len / WAKEUP_STREAM_ONE_FRAME_SIZE;

        for (int i = 0; i < cnt; i ++) {
            app_usb_audio_write((char*)data + i * WAKEUP_STREAM_ONE_FRAME_SIZE, WAKEUP_STREAM_SAMPLE_CNT, WAKEUP_STREAM_CHANNELS, 16);
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
