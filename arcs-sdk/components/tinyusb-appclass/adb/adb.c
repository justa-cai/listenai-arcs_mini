#define LOG_TAG "adb.core"

#include "adb_device.h"
#include "adb.h"
#include "adb_services.h"
#include "adb_utils.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

#include <stdlib.h>

static SemaphoreHandle_t adb_msg_send_lock = NULL;
static SemaphoreHandle_t adb_recv_sem = NULL;
static QueueHandle_t adb_rx_queue = NULL;
static QueueHandle_t adb_tx_queue = NULL;

struct adb_recv_msg {
	uint8_t *data;
	uint32_t len;
};

static uint32_t adb_check_calc(const uint8_t *data, int len)
{
	uint8_t *x = (uint8_t *)data;
	uint32_t sum = 0;
	while (len-- > 0) {
		sum += *x++;
	}

	return sum;
}

static void adb_tx_queue_write(uint8_t *data, uint32_t len)
{
	// struct adb_split sp = {0};
	// sp.len = len;
	// sp.spilt = ADB_MALLOC(sp.len);
	// if (sp.spilt == NULL) {
	// 	ADB_LOGE("ADB_MALLOC error\n");
	// 	return;
	// }
	// memcpy(sp.spilt, data, len);
	// xQueueSend(adb_tx_queue, &sp, portMAX_DELAY);
	adb_dev_send(data, len);
}

static void adb_tx_msg(uint8_t *msg, uint32_t len)
{
#define BULK_PACKET_SIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)
    uint32_t n;

    n = len / BULK_PACKET_SIZE;
    while (n--) {
        adb_tx_queue_write(msg, BULK_PACKET_SIZE);
        msg += BULK_PACKET_SIZE;
    }
    n = len % BULK_PACKET_SIZE;
    if (n) {
        adb_tx_queue_write(msg, n);
    }
}

static int adb_msg_send(struct message *msg, const uint8_t *payload, uint32_t payload_size)
{
    msg->magic = (msg->command ^ 0xffffffff);
    msg->data_length = payload_size;

    if (msg->data_length) {
        msg->data_check = adb_check_calc(payload, msg->data_length);
    } else {
        msg->data_check = 0;
    }

    uint8_t *cmd = (uint8_t *)&msg->command;
    ADB_LOGD("adb msg send, cmd: %c%c%c%c, payload len:%d\n", cmd[0], cmd[1], cmd[2], cmd[3], msg->data_length);

    xSemaphoreTake(adb_msg_send_lock, portMAX_DELAY);
    adb_tx_msg((uint8_t *)msg, sizeof(struct message));
    if (payload_size && payload) {
        adb_tx_msg((uint8_t *)payload, payload_size);
    }
    xSemaphoreGive(adb_msg_send_lock);
	ADB_LOGD("adb msg send done\n");

    return 0;
}

static void adb_connect(void)
{
	const uint8_t conn_payload[] = "device::"
				       "ro.product.name=mido;"
				       "ro.product.model=listenai;"
				       "ro.product.device=mido;"
				       "features=cmd,shell_v1";
	struct message *msg = ADB_MALLOC(sizeof(struct message));
	if (msg == NULL) {
		ADB_LOGE("ADB_MALLOC error\n");
		return;
	}
	memset(msg, 0, sizeof(struct message));

	msg->command = A_CNXN;
	msg->arg0 = A_VERSION;
	msg->arg1 = MAX_PAYLOAD;

	adb_msg_send(msg, conn_payload, sizeof(conn_payload));

	ADB_FREE(msg);
}

static void adb_ready(uint32_t local_id, uint32_t remote_id)
{
	struct message *msg = ADB_MALLOC(sizeof(struct message));
	if (msg == NULL) {
		ADB_LOGE("adb_ready, ADB_MALLOC error\n");
		return;
	}

	msg->command = A_OKAY;
	msg->arg0 = local_id;
	msg->arg1 = remote_id;
	msg->data_length = 0;

	adb_msg_send(msg, NULL, 0);
	ADB_FREE(msg);
}

void adb_close(uint32_t local_id, uint32_t remote_id)
{
	struct message *msg = ADB_MALLOC(sizeof(struct message));
	if (msg == NULL) {
		ADB_LOGE("adb_close, ADB_MALLOC error\n");
		return;
	}

	msg->command = A_CLSE;
	msg->arg0 = local_id;
	msg->arg1 = remote_id;
	msg->data_length = 0;

	adb_msg_send(msg, NULL, 0);
	ADB_FREE(msg);
}

void adb_write(uint32_t local_id, uint32_t remote_id, uint8_t *data, uint32_t len)
{
	struct message *msg = ADB_MALLOC(sizeof(struct message));
	if (msg == NULL) {
        ADB_LOGE("adb_write, ADB_MALLOC error\n");
        return;
    }

    msg->command = A_WRTE;
    msg->arg0 = local_id;
    msg->arg1 = remote_id;
    msg->data_length = len;

    adb_msg_send(msg, data, len);

    ADB_FREE(msg);
}

void adb_packet_free(adb_packet_t *p)
{
	ADB_FREE(p);
}

static void adb_packet_received_cb(adb_packet_t *p)
{
	if (p == NULL) {
		return;
	}
	uint8_t *cmd = (uint8_t *)&p->msg.command;
	ADB_LOGD("adb packet recv, cmd: %c%c%c%c, payload len:%d\n", cmd[0], cmd[1], cmd[2], cmd[3],
		p->msg.data_length);
	switch (p->msg.command) {
	case A_CNXN: {
		ADB_LOGI("adb connecting, remote info, version:%x, max payload:%d\n", p->msg.arg0,
			p->msg.arg1);
		adb_connect();
		adb_packet_free(p);
	} break;
	case A_OPEN: {
		uint32_t remote_id = p->msg.arg0;
		uint8_t *name = p->data;

		ADB_LOGI("adb open, remote_id:%d, name:%s\n", remote_id, name);
		if (remote_id != 0) {
			uint8_t *pos = strstr(name, ":");
			if (pos == NULL) {
				ADB_LOGE("invalid name:%s\n", name);
				adb_close(0, remote_id);
				adb_packet_free(p);
				return;
			}
			*pos = '\0';
			uint32_t local_id = adb_service_open(name, pos + 1, remote_id);
			ADB_LOGI("adb open, local_id:%d\n", local_id);
			if (local_id != 0) {
				adb_ready(local_id, remote_id);
			} else {
				adb_close(0, remote_id);
			}
		}
		adb_packet_free(p);
	} break;
	case A_OKAY:
		adb_packet_free(p);
		break;
	case A_CLSE: {
		uint32_t local_id = p->msg.arg1;
		uint32_t remote_id = p->msg.arg0;

		ADB_LOGI("adb close, local_id:%d, remote_id:%d\n", local_id, remote_id);
		adb_service_close(local_id, remote_id);
		adb_packet_free(p);
	} break;
	case A_WRTE: {
		uint32_t local_id = p->msg.arg1;
		uint32_t remote_id = p->msg.arg0;
		if (local_id == 0 || remote_id == 0) {
			adb_packet_free(p);
			return;
		}

		if (adb_service_write(local_id, remote_id, p) != 0) {
			adb_packet_free(p);
			adb_close(local_id, remote_id);
		} else {
			/* adb packet will be freed in each adb service */
			adb_ready(local_id, remote_id);
		}
	} break;
	default:
		adb_packet_free(p);
		break;
	}
}

static bool adb_msg_validate(struct message *m)
{
    if (m == NULL) {
        return false;
    }

    if (m->magic != (m->command ^ 0xffffffff)) {
        ADB_LOGE("adb msg magic check failed, cmd:%x\n, calc magic:%x", m->command, (m->command ^ 0xffffffff));
        return false;
    }

	if (m->data_length > MAX_PAYLOAD) {
		ADB_LOGE("adb packet data length check failed, len: %d\n", m->data_length);
		return false;
	}

    return true;
}

static bool adb_packet_validate(adb_packet_t *p)
{
	if (p->msg.data_check != adb_check_calc(p->data, p->msg.data_length)) {
		ADB_LOGE("adb packet check sum failed, cmd:%x\n,", p->msg.command);
		return false;
	}

	return true;
}

static void adb_tx_task(void *arg)
{
	struct adb_split sp = {0};

	while (1) {
		xQueueReceive(adb_tx_queue, &sp, portMAX_DELAY);
		adb_dev_send(sp.spilt, sp.len);
	}
}

static void adb_rx_task(void *arg)
{
	struct adb_recv_msg msg;
	uint32_t payload_recv_len = 0;
	adb_packet_t *packet;

    while (1) {
        xQueueReceive(adb_rx_queue, &msg, portMAX_DELAY);

        struct message *m = (struct message *)msg.data;
        if (__builtin_expect(!adb_msg_validate(m), 0)) {
            ADB_LOGE("adb msg validate failed\n");
            ADB_FREE(msg.data);
            continue;
        }

        packet = ADB_MALLOC(sizeof(adb_packet_t) + m->data_length);
        if (packet == NULL) {
            ADB_LOGE("malloc error\n");
            ADB_FREE(msg.data);
            continue;
        }

        memcpy(packet, m, sizeof(struct message));
		ADB_FREE(msg.data);
        payload_recv_len = 0;

        if (__builtin_expect(packet->msg.data_length > 0, 1)) {
            while (payload_recv_len < packet->msg.data_length) {
                xQueueReceive(adb_rx_queue, &msg, portMAX_DELAY);
                memcpy(packet->data + payload_recv_len, msg.data, msg.len);
                payload_recv_len += msg.len;
                ADB_FREE(msg.data);
            }
        }

        if (__builtin_expect(adb_packet_validate(packet), 0)) {
            adb_packet_received_cb(packet);
        }
    }
}

static void adb_recv_handle(uint8_t *buf, uint32_t len)
{
	struct adb_recv_msg msg;

	msg.data = ADB_MALLOC(len);
	if (msg.data == NULL) {
		ADB_LOGE("adb_recv_handle, malloc error\n");
		return;
	}

	msg.len = len;
	memcpy(msg.data, buf, len);
	if (xQueueSend(adb_rx_queue, &msg, portMAX_DELAY) != pdPASS) {
		ADB_LOGE("adb_recv_handle, queue send error\n");
		ADB_FREE(msg.data);
	}
}

void adb_init(void)
{
	adb_dev_recv_cb_set(adb_recv_handle);

	adb_rx_queue = xQueueCreate(50, sizeof(struct adb_recv_msg));
	if (adb_rx_queue == NULL) {
		ADB_LOGE("adb recv queue create failed\n");
		return;
	}

	adb_tx_queue = xQueueCreate(10, sizeof(struct adb_split));
	if (adb_tx_queue == NULL) {
		ADB_LOGE("adb tx queue create failed\n");
		return;
	}

	adb_msg_send_lock = xSemaphoreCreateMutex();
    if (adb_msg_send_lock == NULL) {
        ADB_LOGE("adb mutex create failed\n");
		return;
    }

	if (xTaskCreate(adb_rx_task, "adb_rx", 1024 * 2, NULL, CONFIG_ADB_TASK_PRIORITY, NULL) != pdPASS) {
		ADB_LOGE("adb task create failed\n");
		return;
	}

	if (xTaskCreate(adb_tx_task, "adb_tx", 1024 * 2, NULL, CONFIG_ADB_TASK_PRIORITY, NULL) != pdPASS) {
		ADB_LOGE("adb task create failed\n");
		return;
	}
}
