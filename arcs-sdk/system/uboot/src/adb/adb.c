/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "xutils.h"
#include "usbd_core.h"
#include "adb_services.h"
#include "adb_utils.h"
#include "adb.h"
#include "boot_mem_chunk.h"

#define ADB_EVT_TX_BUSY	BIT(0)
#define ADB_EVT_TX_DONE	BIT(1)

typedef enum { ADB_STAT_XMSG, ADB_STAT_XDAT, ADB_STAT_XZLP, ADB_STAT_WAIT_MEM } adb_state_t;
struct adb_context {
	struct usbd_endpoint txepi, rxepo;
	adb_packet_t *txpkt, *rxpkt;
	QueueHandle_t rxque;
	SemaphoreHandle_t txsem;
	volatile adb_state_t txstat, rxstat;
	uint8_t busid;
};
static struct adb_context *ctx = NULL;

#if 0
static void usbd_dump_packet(const char *title, uint8_t epaddr, const void *const data, uint32_t const size)
{
    #define FMT8(F, V) F F F F F F F F, V[-8], V[-7], V[-6], V[-5], V[-4], V[-3], V[-2], V[-1]    
    #define FMT7(F, V) F F F F F F F         , V[-7], V[-6], V[-5], V[-4], V[-3], V[-2], V[-1]    
    #define FMT6(F, V) F F F F F F                  , V[-6], V[-5], V[-4], V[-3], V[-2], V[-1]
    #define FMT5(F, V) F F F F F                           , V[-5], V[-4], V[-3], V[-2], V[-1]
    #define FMT4(F, V) F F F F                                    , V[-4], V[-3], V[-2], V[-1]
    #define FMT3(F, V) F F F                                             , V[-3], V[-2], V[-1]    
    #define FMT2(F, V) F F                                                      , V[-2], V[-1]
    #define FMT1(F, V) F                                                               , V[-1]

    uint8_t xlen = MIN(size, 16); uint8_t const *xptr = data;
    printf("| %s/ep%02x/%p+%ld: \e[4m", title, epaddr, data, size);
    for (uint8_t i = 0; i < xlen; i++) printf("%02x ", xptr[i]);
    xlen = size - xlen, xptr += size;
    if (xlen > 8) { xlen = 8; printf(".. "); }
    switch (xlen) {
    case 1: printf(FMT1("%02X ", xptr)); break;
    case 2: printf(FMT2("%02X ", xptr)); break;
    case 3: printf(FMT3("%02X ", xptr)); break;
    case 4: printf(FMT4("%02X ", xptr)); break;
    case 5: printf(FMT5("%02X ", xptr)); break;
    case 6: printf(FMT6("%02X ", xptr)); break;
    case 7: printf(FMT7("%02X ", xptr)); break;
    case 8: printf(FMT8("%02X ", xptr)); break;
    }
    printf("\e[0m\r\n");
}
#endif

static inline uint32_t adb_calc_checksum(const uint8_t *data, uint32_t size)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < size; i++)
        sum += data[i];
    return sum;
}

static void usbd_recv_start(adb_state_t stat)
{
	adb_packet_t *newpkt = NULL;
	// adb_packet_t *const oldpkt = ctx->rxpkt;
	switch ((ctx->rxstat = stat)) {
	case ADB_STAT_XMSG:
		// newpkt = ADB_MALLOC(ADB_MSG_SIZE + 36);
		// ADB_ASSERT(newpkt);
		newpkt = adb_packet_get();
		// boot_mem_large_chunk_put(p);
		if (newpkt) {
			ctx->rxpkt = newpkt;
			usbd_ep_start_read(ctx->busid, ctx->rxepo.ep_addr, (void *)newpkt, ADB_MSG_SIZE);
		} else {
			ADB_LOGW("no mem for rxpkt");
			ctx->rxstat = ADB_STAT_WAIT_MEM;
			// __builtin_trap();
		}
		// memset(newpkt, 0, ADB_MSG_SIZE);
		// ADB_LOGW(">> adb rxmsg xreq: addr=%p size=%d", newpkt, ADB_MSG_SIZE);
		// here we alloc extra 36 bytes to hold standard sht ordata packet, if the actual data 
		// length is less than this value, use it directly, otherwise, reallocate it.
		break;
	case ADB_STAT_XDAT:
		// ADB_ASSERT(oldpkt && oldpkt->datlen <= ADB_RX_PAYLOAD);
		// // here, we alloc some extra bytes for sepecial use case, such as string termination
		// newpkt = inram_realloc(oldpkt, ADB_MSG_SIZE + oldpkt->datlen + 4);
		// ADB_ASSERT(newpkt);
		// ADB_LOGW(">> adb rxdat xreq: addr=%p size=%ld", newpkt, newpkt->datlen);
		usbd_ep_start_read(ctx->busid, ctx->rxepo.ep_addr, ctx->rxpkt->data, ctx->rxpkt->datlen);
		break;
	default:
		ADB_ASSERT(false);
		break;
	}
}
static void usbd_send_start(adb_state_t stat)
{
	switch ((ctx->txstat = stat)) {
	case ADB_STAT_XMSG:
		// ADB_LOGI(">> adb txmsg xreq: addr=%p size=%d", ctx->txpkt, ADB_MSG_SIZE);
		usbd_ep_start_write(ctx->busid, ctx->txepi.ep_addr, (void *)ctx->txpkt, ADB_MSG_SIZE);
		break;
	case ADB_STAT_XDAT:
		// ADB_LOGI(">> adb txdat xreq: addr=%p+24 size=%d", ctx->txpkt, ctx->txpkt->datlen);
		usbd_ep_start_write(ctx->busid, ctx->txepi.ep_addr, ctx->txpkt->data, ctx->txpkt->datlen);
		break;
	case ADB_STAT_XZLP:
		// ADB_LOGI(">> adb txzlp xreq");
		usbd_ep_start_write(ctx->busid, ctx->txepi.ep_addr, NULL, 0);
		break;
	default:
		ADB_ASSERT(false);
		break;
	}
}

static void usbd_epi_send_done(uint8_t busid, uint8_t edpt, uint32_t nbytes)
{
	BaseType_t yield = pdFALSE, ret;

	portENTER_CRITICAL();

    if (ctx->rxstat == ADB_STAT_WAIT_MEM) {
		ADB_LOGW("adb rx waiting mem, restart receive\n");
        usbd_recv_start(ADB_STAT_XMSG);
    }

    portEXIT_CRITICAL();

	switch (ctx->txstat) {
	case ADB_STAT_XMSG:
		// ADB_LOGI("<< adb txmsg done: addr=%p size=%ld xreq=%ld", ctx->txpkt, nbytes, ctx->txpkt->datlen);
		if (ctx->txpkt->datlen > 0) {
			usbd_send_start(ADB_STAT_XDAT);
			break;
		}
	case ADB_STAT_XDAT:
		// if (ctx->txpkt->datlen == ADB_TX_PAYLOAD) {
			// ADB_LOGI("<< adb txdat done: addr=%p size=%ld xreq=%ld", ctx->txpkt, nbytes, ctx->txpkt->datlen);
			// usbd_send_start(ADB_STAT_XZLP);
			// break;
		// }
	case ADB_STAT_XZLP:
		ret = xSemaphoreGiveFromISR(ctx->txsem, &yield);
		ADB_ASSERT(ret == pdPASS);
		portYIELD_FROM_ISR(yield);
		break;
	default:
		ADB_ASSERT(false);
		break;
	}
}
static void usbd_epo_recv_done(uint8_t busid, uint8_t edpt, uint32_t nbytes)
{
	adb_packet_t *const rxpkt = ctx->rxpkt;
	uint32_t remote = rxpkt->arg0, local = rxpkt->arg1;
	BaseType_t yield = pdFALSE;
	BaseType_t ret = pdTRUE;

	ADB_ASSERT(rxpkt);

	const char *xcmd = (char *)&rxpkt->mcmd;
	switch (ctx->rxstat) {
	case ADB_STAT_XMSG:
		// ADB_LOGW("<< adb rxmsg done: addr=%p size=%ld xreq=%ld ", rxpkt, nbytes, rxpkt->datlen);
		ADB_ASSERT(!nbytes || nbytes == ADB_MSG_SIZE);
		if (nbytes && rxpkt->datlen)
			usbd_recv_start(ADB_STAT_XDAT);
		else {
			usbd_recv_start(ADB_STAT_XMSG);
			switch (rxpkt->mcmd) {
			case ADB_MAIN_ID_CLSE:
			case ADB_MAIN_ID_OKAY:
				ret = xQueueSendFromISR(ctx->rxque, &rxpkt, &yield);
				ADB_ASSERT(ret == pdPASS);
				portYIELD_FROM_ISR(yield);
				break; // free in task or service context
			default:
				if (nbytes) ADB_LOGI("<< adb rxmsg: cmd=%c%c%c%c", xcmd[0], xcmd[1], xcmd[2], xcmd[3]);
				adb_packet_free(rxpkt);
				break;
			}
		}
		break;
	case ADB_STAT_XDAT:
		// ADB_LOGW("<< adb rxdat done: addr=%p size=%ld xreq=%ld", rxpkt, nbytes, rxpkt->datlen);
		// rxpkt->data[rxpkt->datlen] = 0; // we have alloc 4 more bytes already, so just null-terminate it
		if (nbytes != rxpkt->datlen) {
			ADB_LOGE("nbytes: %d is not equal %d", nbytes, rxpkt->datlen);
			__builtin_trap();
		}
		usbd_recv_start(ADB_STAT_XMSG);
		switch (rxpkt->mcmd) {
		case ADB_MAIN_ID_CNXN:
		case ADB_MAIN_ID_OPEN:
		case ADB_MAIN_ID_WRTE:
			ret = xQueueSendFromISR(ctx->rxque, &rxpkt, &yield);
			ADB_ASSERT(ret == pdPASS);
			portYIELD_FROM_ISR(yield);
			break; // free in task or service context
		default:
			ADB_LOGI("<< adb rxdat: cmd=%c%c%c%c", xcmd[0], xcmd[1], xcmd[2], xcmd[3]);
			adb_packet_free(rxpkt);
			break;
        }
		break;
	default:
		ADB_ASSERT(false);
		break;
	}
}

static void usbd_ep0_notify(uint8_t busid, uint8_t event, void *arg)
{
	if (event == USBD_EVENT_CONFIGURED) usbd_recv_start(ADB_STAT_XMSG);
}

static void adb_send_cnxn(adb_packet_t *txpkt, const char *feature)
{
	xSemaphoreTake(ctx->txsem, portMAX_DELAY);
	txpkt->magic = ~(txpkt->mcmd = ADB_MAIN_ID_CNXN);
	txpkt->arg0 = ADB_VERSION;
	txpkt->arg1 = ADB_RX_PAYLOAD;
	txpkt->datlen = strlen(feature);
	memcpy(txpkt->data, feature, txpkt->datlen);
    txpkt->datsum = adb_calc_checksum(txpkt->data, txpkt->datlen);
	usbd_send_start(ADB_STAT_XMSG);
}
static void adb_send_okay(adb_packet_t *txpkt, uint32_t local, uint32_t remote)
{
	xSemaphoreTake(ctx->txsem, portMAX_DELAY);
	txpkt->magic = ~(txpkt->mcmd = ADB_MAIN_ID_OKAY);
	txpkt->arg0 = local;
	txpkt->arg1 = remote;
	txpkt->datlen = 0;
	txpkt->datsum = 0;
	usbd_send_start(ADB_STAT_XMSG);
}
static void adb_send_clse(adb_packet_t *txpkt, uint32_t local, uint32_t remote)
{
	xSemaphoreTake(ctx->txsem, portMAX_DELAY);
	txpkt->magic = ~(txpkt->mcmd = ADB_MAIN_ID_CLSE);
    txpkt->arg0 = local;
    txpkt->arg1 = remote;
    txpkt->datlen = 0;
    txpkt->datsum = 0;
	usbd_send_start(ADB_STAT_XMSG);
}
static void adb_send_wrte(adb_packet_t *txpkt, uint32_t local, uint32_t remote, const uint8_t *data, uint32_t size)
{
	xSemaphoreTake(ctx->txsem, portMAX_DELAY);
	txpkt->magic = ~(txpkt->mcmd = ADB_MAIN_ID_WRTE);
   	txpkt->arg0 = local;
   	txpkt->arg1 = remote;
   	txpkt->datlen = size;
	memcpy(txpkt->data, data, txpkt->datlen);
	txpkt->datsum = adb_calc_checksum(txpkt->data, txpkt->datlen);
	usbd_send_start(ADB_STAT_XMSG);
}

static void adb_recv_task(void *args)
{
	ADB_LOGD("adb rxtask enter");
	while (true) {
		// ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		adb_packet_t *rxpkt = NULL;
		xQueueReceive(ctx->rxque, &rxpkt, portMAX_DELAY);
		if (!rxpkt) break;

		// uint32_t chksum = adb_calc_checksum(rxpkt->data, rxpkt->datlen);
		// if (chksum != rxpkt->datsum) ADB_LOGW("checksum mismatch");

		uint32_t remote = rxpkt->arg0, local = rxpkt->arg1;
		switch (rxpkt->mcmd) {
		case ADB_MAIN_ID_CNXN:
			ADB_LOGI("CNXN: ver %#lx, mps %ldkb, %s", remote, local/1024, (char *)rxpkt->data);
			adb_send_cnxn(ctx->txpkt, ADB_CNXN_FEAT);
			break;
		case ADB_MAIN_ID_OPEN:
			ADB_LOGI("OPEN: cmd='%s', remote=%ld", (char *)rxpkt->data, remote);
			char *name = (void *)rxpkt->data, *args = "";
			int arglen = rxpkt->datlen - 1;
			for (int i = 0; i < arglen; i++)
				if (name[i] == ':') {
					name[i] = 0;
					args = name + i + 1;
					break;
				}
			if ((local = adb_service_open(name, args, remote)))
				adb_send_okay(ctx->txpkt, local, remote);
			else {
				ADB_LOGI("open: fail, close(local=%ld remote=%ld)", local, remote);
				adb_send_clse(ctx->txpkt, local, remote);
			}
			break;
		case ADB_MAIN_ID_WRTE:
			// ADB_LOGI("WRTE: local=%ld remote=%ld", local, remote);
			if (!adb_service_write(local, remote, rxpkt)) {
				adb_send_okay(ctx->txpkt, local, remote);
				continue; // free memory in adb service context
			}
			// service will been closed by framework if failed
			ADB_LOGE("write: fail, close(local=%ld remote=%ld)", local, remote);
			break;
		case ADB_MAIN_ID_CLSE:
			ADB_LOGI("CLSE: local=%ld remote=%ld", local, remote);
			adb_service_close(local, remote);
			break;
		case ADB_MAIN_ID_OKAY:
			// ADB_LOGI("OKAY: load=%ld remote=%ld", local, remote);
			break;
		default:
			ADB_ASSERT(false);
			break;
		}
		adb_packet_free(rxpkt);
	}
	ADB_LOGD("adb rxtask exit");

	vQueueDelete(ctx->rxque);
	vTaskDelete(NULL);
}

static void adb_wait_done(void)
{
	xSemaphoreTake(ctx->txsem, portMAX_DELAY);
	xSemaphoreGive(ctx->txsem);
}

void adb_write(uint32_t local, uint32_t remote, uint8_t *data, uint32_t size)
{
	// ADB_LOGW("client: send(wrte+%ld)", size);
	adb_send_wrte(ctx->txpkt, local, remote, data, size);
	adb_wait_done();
}

void adb_close(uint32_t local, uint32_t remote)
{
	// ADB_LOGW("client: send(clse)");
	adb_send_clse(ctx->txpkt, local, remote);
	adb_wait_done();
}

adb_packet_t *adb_packet_get()
{
    return (adb_packet_t *)boot_mem_large_chunk_get(0);
}

void adb_packet_free(adb_packet_t *pkt)
{
    ADB_LOGI("adb packet free @ %p\n", pkt);
    // ADB_FREE(pkt);
    boot_mem_large_chunk_put(pkt);

    portENTER_CRITICAL();

    if (ctx->rxstat == ADB_STAT_WAIT_MEM) {
		ADB_LOGW("adb rx waiting mem, restart receive\n");
        usbd_recv_start(ADB_STAT_XMSG);
    }

    portEXIT_CRITICAL();
}

struct usbd_interface *usbd_adb_init_intf(uint8_t busid, struct usbd_interface *intf, uint8_t in_ep, uint8_t out_ep)
{
	ctx = inram_calloc(CONFIG_USB_ALIGN_SIZE, 1, sizeof(*ctx));
	ADB_ASSERT(ctx);
	ctx->txpkt = ADB_MALLOC(sizeof(adb_packet_t) + ADB_TX_PAYLOAD);
	ADB_ASSERT(ctx->txpkt);
	ctx->rxpkt = NULL;
	ctx->txsem = xSemaphoreCreateBinary();
	ADB_ASSERT(ctx->txsem);
	xSemaphoreGive(ctx->txsem);
	ctx->busid = busid;
	ctx->rxepo.ep_cb = usbd_epo_recv_done;
    ctx->rxepo.ep_addr = out_ep;
	ctx->txepi.ep_cb = usbd_epi_send_done;
    ctx->txepi.ep_addr = in_ep;
	ctx->txstat = ADB_STAT_XMSG;
	ctx->rxstat = ADB_STAT_XMSG;
	ctx->rxque = xQueueCreate(MEM_CHUNK_COUNT, sizeof(adb_packet_t *));
	ADB_ASSERT(ctx->rxque);
	BaseType_t ret = xTaskCreate(adb_recv_task, "adbd", OS_STACK_DEF, ctx, OS_PRIO_DEF, NULL);
	ADB_ASSERT(pdTRUE == ret);

    intf->class_interface_handler = NULL;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = usbd_ep0_notify;

    usbd_add_endpoint(ctx->busid, &ctx->rxepo);
    usbd_add_endpoint(ctx->busid, &ctx->txepi);

    return intf;
}
