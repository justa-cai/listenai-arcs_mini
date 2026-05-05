#ifndef __ADB_DEVICE_H__
#define __ADB_DEVICE_H__

#include "tusb.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#define ADB_BUFFER_SIZE (1024 * 5)
#define ADB_BULK_PACKET_SIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define ADB_EP_OUT_XFER_BUFSIZE ((CONFIG_ADB_MAX_PAYLOAD_SIZE) < 65024 ? (CONFIG_ADB_MAX_PAYLOAD_SIZE) : 65024)

#if defined(CONFIG_BOOT_ADB)
#define ADB_EP_OUT_BUFSIZE  ADB_EP_OUT_XFER_BUFSIZE
#define ADB_EP_IN_BUFSIZE   ADB_BULK_PACKET_SIZE
#else
#define ADB_EP_OUT_BUFSIZE  ADB_BULK_PACKET_SIZE
#define ADB_EP_IN_BUFSIZE   ADB_BULK_PACKET_SIZE
#endif

typedef struct {
	uint8_t itf_num;
	uint8_t ep_in;
	uint8_t ep_out;
    uint8_t *rx_buf;
    uint32_t rx_len;

	CFG_TUSB_MEM_ALIGN uint8_t epout_buf[ADB_EP_OUT_BUFSIZE];
	CFG_TUSB_MEM_ALIGN uint8_t epin_buf[ADB_EP_IN_BUFSIZE];

	SemaphoreHandle_t tx_ready_sem;
} adb_interface_t;

void adb_dev_init(void);
bool adb_dev_deinit(void);
void adb_dev_reset(uint8_t rhport);
uint16_t adb_dev_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len);
bool adb_dev_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request);
bool adb_dev_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

uint32_t adb_dev_write(void const *buffer, uint32_t bufsize);
void adb_dev_recv_cb_set(void (*cb)(uint8_t*, uint32_t));
bool adb_dev_send(uint8_t *buf, uint32_t len);
bool adb_dev_recv_arm(uint8_t *buf, uint32_t len);

#endif
