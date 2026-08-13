#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "arcs_ap.h"
#include "arcs_ap_base.h"
#include "usbd_core.h"
#include "usbd_audio.h"

#include "cae_uac.h"

#define TAG "cae_uac"
#include "lisa_log.h"

#define CAE_UAC_BUS_ID              0
#define CAE_UAC_IN_EP               0x81
#define CAE_UAC_IN_FU_ID            0x02
#define CAE_UAC_SAMPLE_RATE         16000U
#define CAE_UAC_FRAME_SIZE_BYTE     2U
#define CAE_UAC_RESOLUTION_BIT      16U
#define CAE_UAC_CHANNELS            1U
#define CAE_UAC_PACKET_BYTES        ((CAE_UAC_SAMPLE_RATE * CAE_UAC_FRAME_SIZE_BYTE * CAE_UAC_CHANNELS) / 1000U)
#define CAE_UAC_RING_BYTES          (CAE_UAC_PACKET_BYTES * 512U)
#define CAE_UAC_TASK_STACK          2048
#define CAE_UAC_TASK_PRIO           8
#define CAE_UAC_VOLUME_DEFAULT_DB   0

#define USBD_VID                    0xffff
#define USBD_PID                    0xffff
#define USBD_MAX_POWER              100

#ifdef CONFIG_USB_HS
#define CAE_UAC_EP_INTERVAL         0x04
#else
#define CAE_UAC_EP_INTERVAL         0x01
#endif

#define CAE_UAC_INPUT_CTRL          0x03, 0x03
#define CAE_UAC_INPUT_CH_ENABLE     0x0001

#define CAE_UAC_CONFIG_SIZE (unsigned long)(9 +                                          \
                                           AUDIO_AC_DESCRIPTOR_LEN(1) +                  \
                                           AUDIO_SIZEOF_AC_INPUT_TERMINAL_DESC +         \
                                           AUDIO_SIZEOF_AC_FEATURE_UNIT_DESC(CAE_UAC_CHANNELS, 1) + \
                                           AUDIO_SIZEOF_AC_OUTPUT_TERMINAL_DESC +        \
                                           AUDIO_AS_DESCRIPTOR_LEN(1))

#define CAE_UAC_AC_SIZE (AUDIO_SIZEOF_AC_HEADER_DESC(1) +                                \
                         AUDIO_SIZEOF_AC_INPUT_TERMINAL_DESC +                          \
                         AUDIO_SIZEOF_AC_FEATURE_UNIT_DESC(CAE_UAC_CHANNELS, 1) +        \
                         AUDIO_SIZEOF_AC_OUTPUT_TERMINAL_DESC)

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xef, 0x02, 0x01, USBD_VID, USBD_PID, 0x0001, 0x01)
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(CAE_UAC_CONFIG_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    AUDIO_AC_DESCRIPTOR_INIT(0x00, 0x02, CAE_UAC_AC_SIZE, 0x00, 0x01),
    AUDIO_AC_INPUT_TERMINAL_DESCRIPTOR_INIT(0x01, AUDIO_INTERM_MIC, CAE_UAC_CHANNELS, CAE_UAC_INPUT_CH_ENABLE),
    AUDIO_AC_FEATURE_UNIT_DESCRIPTOR_INIT(CAE_UAC_IN_FU_ID, 0x01, 0x01, CAE_UAC_INPUT_CTRL),
    AUDIO_AC_OUTPUT_TERMINAL_DESCRIPTOR_INIT(0x03, AUDIO_TERMINAL_STREAMING, CAE_UAC_IN_FU_ID),
    AUDIO_AS_DESCRIPTOR_INIT(0x01, 0x03, CAE_UAC_CHANNELS, CAE_UAC_FRAME_SIZE_BYTE,
                             CAE_UAC_RESOLUTION_BIT, CAE_UAC_IN_EP, 0x05,
                             CAE_UAC_PACKET_BYTES, CAE_UAC_EP_INTERVAL,
                             AUDIO_SAMPLE_FREQ_3B(CAE_UAC_SAMPLE_RATE))
};

static const uint8_t device_quality_descriptor[] = {
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 },
    "ListenAI",
    "ListenAI CAE UAC",
    "2026050901",
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void)speed;

    if (index >= (sizeof(string_descriptors) / sizeof(string_descriptors[0]))) {
        return NULL;
    }

    return string_descriptors[index];
}

static const struct usb_descriptor cae_uac_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
};

static struct usbd_interface audio_control_intf;
static struct usbd_interface audio_stream_intf;
static struct audio_entity_info audio_entity_table[] = {
    {
        .bEntityId = CAE_UAC_IN_FU_ID,
        .bDescriptorSubtype = AUDIO_CONTROL_FEATURE_UNIT,
        .ep = CAE_UAC_IN_EP,
    },
};

static volatile bool uac_opened;
static volatile bool uac_ep_busy;
static volatile bool uac_started;
static volatile uint32_t uac_sample_rate = CAE_UAC_SAMPLE_RATE;
static bool uac_mute;
static int uac_volume_db = CAE_UAC_VOLUME_DEFAULT_DB;

static uint8_t uac_ring[CAE_UAC_RING_BYTES];
static uint32_t uac_ring_rd;
static uint32_t uac_ring_wr;
static uint32_t uac_ring_used;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX static uint8_t uac_tx_packet[CAE_UAC_PACKET_BYTES];

static void uac_ring_push(const uint8_t *data, uint32_t len)
{
    taskENTER_CRITICAL();

    if (len > (sizeof(uac_ring) - uac_ring_used)) {
        uint32_t drop = len - (sizeof(uac_ring) - uac_ring_used);
        if (drop > uac_ring_used) {
            drop = uac_ring_used;
        }
        uac_ring_rd = (uac_ring_rd + drop) % sizeof(uac_ring);
        uac_ring_used -= drop;
    }

    for (uint32_t i = 0; i < len; i++) {
        uac_ring[uac_ring_wr] = data[i];
        uac_ring_wr = (uac_ring_wr + 1U) % sizeof(uac_ring);
    }
    uac_ring_used += len;

    taskEXIT_CRITICAL();
}

static uint32_t uac_ring_pop(uint8_t *data, uint32_t len)
{
    uint32_t copied = 0;

    taskENTER_CRITICAL();

    while (copied < len && uac_ring_used > 0) {
        data[copied++] = uac_ring[uac_ring_rd];
        uac_ring_rd = (uac_ring_rd + 1U) % sizeof(uac_ring);
        uac_ring_used--;
    }

    taskEXIT_CRITICAL();

    return copied;
}

static void uac_ring_reset(void)
{
    taskENTER_CRITICAL();
    uac_ring_rd = 0;
    uac_ring_wr = 0;
    uac_ring_used = 0;
    taskEXIT_CRITICAL();
}

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    (void)busid;

    switch (event) {
    case USBD_EVENT_RESET:
        LISA_LOGI(TAG, "USB reset");
        break;
    case USBD_EVENT_CONNECTED:
        LISA_LOGI(TAG, "USB connected");
        break;
    case USBD_EVENT_DISCONNECTED:
        LISA_LOGI(TAG, "USB disconnected");
        uac_opened = false;
        uac_ep_busy = false;
        uac_ring_reset();
        break;
    case USBD_EVENT_CONFIGURED:
        LISA_LOGI(TAG, "USB configured");
        break;
    default:
        break;
    }
}

void usbd_audio_open(uint8_t busid, uint8_t intf)
{
    (void)busid;
    (void)intf;

    uac_ring_reset();
    uac_ep_busy = false;
    uac_opened = true;
    LISA_LOGI(TAG, "UAC opened");
}

void usbd_audio_close(uint8_t busid, uint8_t intf)
{
    (void)busid;
    (void)intf;

    uac_opened = false;
    uac_ep_busy = false;
    uac_ring_reset();
    LISA_LOGI(TAG, "UAC closed");
}

void usbd_audio_set_volume(uint8_t busid, uint8_t ep, uint8_t ch, int volume_db)
{
    (void)busid;
    (void)ep;
    (void)ch;

    uac_volume_db = volume_db;
}

int usbd_audio_get_volume(uint8_t busid, uint8_t ep, uint8_t ch)
{
    (void)busid;
    (void)ep;
    (void)ch;

    return uac_volume_db;
}

void usbd_audio_set_mute(uint8_t busid, uint8_t ep, uint8_t ch, bool mute)
{
    (void)busid;
    (void)ep;
    (void)ch;

    uac_mute = mute;
}

bool usbd_audio_get_mute(uint8_t busid, uint8_t ep, uint8_t ch)
{
    (void)busid;
    (void)ep;
    (void)ch;

    return uac_mute;
}

void usbd_audio_set_sampling_freq(uint8_t busid, uint8_t ep, uint32_t sampling_freq)
{
    (void)busid;

    if (ep == CAE_UAC_IN_EP) {
        uac_sample_rate = sampling_freq;
    }
}

uint32_t usbd_audio_get_sampling_freq(uint8_t busid, uint8_t ep)
{
    (void)busid;

    if (ep == CAE_UAC_IN_EP) {
        return uac_sample_rate;
    }

    return 0;
}

static void usbd_audio_iso_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;

    uac_ep_busy = false;
}

static struct usbd_endpoint audio_in_ep = {
    .ep_cb = usbd_audio_iso_callback,
    .ep_addr = CAE_UAC_IN_EP,
};

static void cae_uac_task(void *arg)
{
    (void)arg;

    while (1) {
        if (!uac_opened || uac_ep_busy) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        memset(uac_tx_packet, 0, sizeof(uac_tx_packet));
        (void)uac_ring_pop(uac_tx_packet, sizeof(uac_tx_packet));

        uac_ep_busy = true;
        int ret = usbd_ep_start_write(CAE_UAC_BUS_ID, CAE_UAC_IN_EP, uac_tx_packet, sizeof(uac_tx_packet));
        if (ret < 0) {
            uac_ep_busy = false;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

uint32_t cae_uac_write(const void *data, uint32_t len)
{
    if (!uac_started || data == NULL || len == 0) {
        return 0;
    }

    uac_ring_push((const uint8_t *)data, len);
    return len;
}

int cae_uac_init(void)
{
    if (uac_started) {
        return 0;
    }

    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;

    usbd_desc_register(CAE_UAC_BUS_ID, &cae_uac_descriptor);
    usbd_add_interface(CAE_UAC_BUS_ID,
                       usbd_audio_init_intf(CAE_UAC_BUS_ID, &audio_control_intf,
                                            0x0100, audio_entity_table, 1));
    usbd_add_interface(CAE_UAC_BUS_ID,
                       usbd_audio_init_intf(CAE_UAC_BUS_ID, &audio_stream_intf,
                                            0x0100, audio_entity_table, 1));
    usbd_add_endpoint(CAE_UAC_BUS_ID, &audio_in_ep);
    usbd_initialize(CAE_UAC_BUS_ID, USBC_BASE, usbd_event_handler);

    if (xTaskCreate(cae_uac_task, "cae_uac", CAE_UAC_TASK_STACK, NULL,
                    CAE_UAC_TASK_PRIO, NULL) != pdPASS) {
        LISA_LOGE(TAG, "Failed to create UAC task");
        return -1;
    }

    uac_started = true;
    LISA_LOGI(TAG, "UAC microphone started, %uHz/%ubit/%uch",
              CAE_UAC_SAMPLE_RATE, CAE_UAC_RESOLUTION_BIT, CAE_UAC_CHANNELS);
    return 0;
}
