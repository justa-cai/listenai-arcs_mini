#ifndef APP_USB_CHERRY_H
#define APP_USB_CHERRY_H

#include <stdbool.h>
#include <stdint.h>

#define APP_USB_BUS_ID 0U

#define APP_USB_MSC_OUT_EP 0x01U
#define APP_USB_MSC_IN_EP  0x81U

#define APP_USB_ADB_OUT_EP 0x02U
#define APP_USB_ADB_IN_EP  0x82U

/* ARCS MUSB EP5 has a 64-byte FIFO; UAC2 needs up to 136 bytes. */
#define APP_USB_AUDIO_IN_EP           0x84U
#define APP_USB_AUDIO_STREAM_ITF      2U
#define APP_USB_AUDIO_CHANNELS        4U
#define APP_USB_AUDIO_SAMPLE_RATE     16000U
#define APP_USB_AUDIO_SAMPLE_BITS     16U
#define APP_USB_AUDIO_PACKET_BYTES    128U
#define APP_USB_AUDIO_MAX_PACKET_SIZE APP_USB_AUDIO_PACKET_BYTES

bool app_usb_msc_enabled(void);
void app_usb_prepare_reboot(void);
void app_usb_cherry_descriptors_register(bool msc_mode);

#if defined(CONFIG_APP_USB_AUDIO_ENABLE) && CONFIG_APP_USB_AUDIO_ENABLE
int app_usb_audio_run(void);
void app_usb_cherry_audio_register_endpoint(uint8_t busid);
void app_usb_cherry_audio_on_disconnect(void);
#endif

#endif /* APP_USB_CHERRY_H */
