#ifndef __ADB_DEVICE_H__
#define __ADB_DEVICE_H__

#include <stdbool.h>
#include <stdint.h>

#include "adb.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "usbd_core.h"

#ifndef TUD_OPT_HIGH_SPEED
#define TUD_OPT_HIGH_SPEED 1
#endif

#ifdef CONFIG_USB_HS
#define ADB_BULK_PACKET_SIZE 512U
#else
#define ADB_BULK_PACKET_SIZE 64U
#endif

#define ADB_MESSAGE_SIZE ((uint32_t)sizeof(struct message))
#ifdef CONFIG_BOOT_ADB
/*
 * Keep a full boot WRTE payload in one USB read to avoid re-arming the MUSB
 * OUT transfer in the middle of a 64 KiB packet stream.
 */
#define ADB_MAX_DATA_XFER_BUFSIZE MAX_PAYLOAD
/*
 * The drop buffer is only used when packet allocation fails and we need to
 * drain the current WRTE payload. A smaller scratch buffer saves boot SRAM
 * without changing the normal fast path.
 */
#define ADB_MAX_DROP_XFER_BUFSIZE (8U * 1024U)
#else
#define ADB_MAX_DATA_XFER_BUFSIZE MAX_PAYLOAD
#ifdef CONFIG_ADB_PERF_MODE
#define ADB_MAX_DROP_XFER_BUFSIZE MAX_PAYLOAD
#else
/* Default: one USB bulk packet — matches pre-refactor static SRAM footprint. */
#define ADB_MAX_DROP_XFER_BUFSIZE ADB_BULK_PACKET_SIZE
#endif
#endif
#define ADB_EP_IN_BUFSIZE ADB_BULK_PACKET_SIZE

typedef void (*adb_dev_recv_cb_t)(adb_packet_t *packet);

struct usbd_interface *adb_dev_init_intf(uint8_t busid,
                                         struct usbd_interface *intf,
                                         uint8_t in_ep, uint8_t out_ep);

void adb_dev_init(void);
bool adb_dev_deinit(void);
void adb_dev_reset(uint8_t rhport);
void adb_dev_notify_packet_free(void);

void adb_dev_recv_cb_set(adb_dev_recv_cb_t cb);
bool adb_dev_send(uint8_t *buf, uint32_t len);

#endif
