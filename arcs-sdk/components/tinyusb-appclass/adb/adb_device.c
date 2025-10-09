
#define LOG_TAG "adb.dev"

#include "adb_utils.h"
#include "adb_device.h"
#include "tusb.h"
#include "usbd_pvt.h"

#include <string.h>
#include <stdlib.h>

#define BULK_PACKET_SIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)

static adb_interface_t adb_itf = {0};

bool adb_dev_send(uint8_t *buf, uint32_t len)
{
    xSemaphoreTake(adb_itf.tx_ready_sem, portMAX_DELAY);

    if (!usbd_edpt_claim(0, adb_itf.ep_in)) {
        return false;
    }

    return usbd_edpt_xfer(0, adb_itf.ep_in, buf, len);
}

void adb_dev_init(void)
{
    memset(&adb_itf, 0, sizeof(adb_interface_t));

    adb_itf.tx_ready_sem = xSemaphoreCreateBinary();
    if (adb_itf.tx_ready_sem == NULL) {
        ADB_LOGE("Failed to create ADB tx semaphore\n");
        return;
    }

    /* give initial value */
    xSemaphoreGive(adb_itf.tx_ready_sem);
}

bool adb_dev_deinit(void)
{
    memset(&adb_itf, 0, sizeof(adb_interface_t));

    return true;
}

void adb_dev_reset(uint8_t rhport)
{
    /* give initial value */
    xSemaphoreGive(adb_itf.tx_ready_sem);
}

uint16_t adb_dev_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len)
{
    ADB_LOGI("Opening ADB interface, number: %d, endpoints: %d\n", itf_desc->bInterfaceNumber, itf_desc->bNumEndpoints);

    TU_VERIFY(itf_desc->bInterfaceClass == TUSB_CLASS_VENDOR_SPECIFIC, 0);

    adb_itf.itf_num = itf_desc->bInterfaceNumber;

    uint8_t const *p_desc = tu_desc_next(itf_desc);
    uint8_t const *desc_end = p_desc + max_len;
    uint8_t found_ep = 0;

    while (found_ep < itf_desc->bNumEndpoints) {
        while ((TUSB_DESC_ENDPOINT != tu_desc_type(p_desc)) && (p_desc < desc_end)) {
            p_desc = tu_desc_next(p_desc);
        }
        TU_VERIFY(p_desc < desc_end, 0);

        tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *)p_desc;

        TU_ASSERT(usbd_edpt_open(rhport, desc_ep));
        found_ep++;

        if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
            adb_itf.ep_in = desc_ep->bEndpointAddress;
        } else {
            adb_itf.ep_out = desc_ep->bEndpointAddress;
        }

        p_desc = tu_desc_next(p_desc);
    }

    ADB_LOGI("Configured ADB interface: IN EP 0x%02x, OUT EP 0x%02x\n", adb_itf.ep_in, adb_itf.ep_out);

    if (!usbd_edpt_xfer(rhport, adb_itf.ep_out, adb_itf.epout_buf, ADB_EP_BUFSIZE)) {
        ADB_LOGE("Failed to xfer OUT EP 0x%02x\n", adb_itf.ep_out);
    }

    return (uint16_t)((uintptr_t)p_desc - (uintptr_t)itf_desc);
}

bool adb_dev_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    ADB_LOGI("adb_control_xfer_cb, stage: %d\n", stage);
    adb_interface_t *adb = &adb_itf;
    if ((request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) &&
        (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_ENDPOINT) &&
        (request->bRequest == TUSB_REQ_CLEAR_FEATURE) && (request->wValue == TUSB_REQ_FEATURE_EDPT_HALT)) {
        uint32_t ep_addr = (request->wIndex);

        if (ep_addr == adb->ep_out) {
            if (usbd_edpt_stalled(rhport, (uint8_t)ep_addr)) {
                usbd_edpt_clear_stall(rhport, (uint8_t)ep_addr);
            }
        } else if (ep_addr == adb->ep_in) {
            if (usbd_edpt_stalled(rhport, (uint8_t)ep_addr)) {
                usbd_edpt_clear_stall(rhport, (uint8_t)ep_addr);
            }
        } else {
            return false;
        }
        return true;
    }

    ADB_LOGE("Unhandled control transfer: type=0x%02x, request=0x%02x\n", request->bmRequestType, request->bRequest);
    return false;
}
static void (*adb_dev_recv_cb)(uint8_t*, uint32_t) = NULL;

void adb_dev_recv_cb_set(void (*cb)(uint8_t*, uint32_t))
{
    adb_dev_recv_cb = cb;
}

bool adb_dev_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    adb_interface_t *adb = &adb_itf;
    int r;

    if (ep_addr != adb->ep_in && ep_addr != adb->ep_out) {
        return true;
    }

    if (result != XFER_RESULT_SUCCESS) {
        ADB_LOGE("Transfer error on EP 0x%02x\n", ep_addr);
        return true;
    }

    if (ep_addr == adb->ep_in) {
        ADB_LOGD("TX completed: %d bytes\n", xferred_bytes);
        xSemaphoreGive(adb->tx_ready_sem);
    }

    if (ep_addr == adb->ep_out) {
        ADB_LOGD("RX completed: %d bytes\n", xferred_bytes);
        if (adb_dev_recv_cb) {
            adb_dev_recv_cb(adb->epout_buf, (uint16_t)xferred_bytes);
        }

        bool ret = usbd_edpt_xfer(rhport, adb->ep_out, adb->epout_buf, ADB_EP_BUFSIZE);
        assert(ret);
    }

    return true;
}
