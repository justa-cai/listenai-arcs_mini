#define LOG_TAG "adb.core"

#include "adb_device.h"
#include "adb.h"
#include "adb_services.h"
#include "adb_utils.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include <string.h>

static SemaphoreHandle_t adb_msg_send_lock = NULL;
static uint32_t adb_remote_max_payload = MAX_PAYLOAD;

#ifdef CONFIG_BOOT_ADB
#if (MAX_PAYLOAD < (64U * 1024U))
#define ADB_BOOT_HOST_MAX_PAYLOAD MAX_PAYLOAD
#else
#define ADB_BOOT_HOST_MAX_PAYLOAD (64U * 1024U)
#endif
#else
#define ADB_BOOT_HOST_MAX_PAYLOAD MAX_PAYLOAD
#endif

static uint32_t adb_check_calc(const uint8_t *data, int len)
{
    const uint8_t *x = data;
    uint32_t sum = 0;

    while (len-- > 0) {
        sum += *x++;
    }

    return sum;
}

static void adb_tx_msg(uint8_t *msg, uint32_t len)
{
    uint32_t packet_size = ADB_BULK_PACKET_SIZE;
    uint32_t chunk_len;

    while (len != 0U) {
        chunk_len = len;
        if (chunk_len > packet_size) {
            chunk_len = packet_size;
        }

        adb_dev_send(msg, chunk_len);
        msg += chunk_len;
        len -= chunk_len;
    }
}

static int adb_msg_send(struct message *msg, const uint8_t *payload, uint32_t payload_size)
{
    uint8_t *cmd = (uint8_t *)&msg->command;
    int ret = 0;

    msg->magic = (msg->command ^ 0xffffffffU);
    msg->data_length = payload_size;
    msg->data_check = payload_size != 0U ? adb_check_calc(payload, (int)payload_size) : 0U;

    ADB_LOGD("adb msg send, cmd: %c%c%c%c, payload len:%lu\n",
             cmd[0], cmd[1], cmd[2], cmd[3], (unsigned long)msg->data_length);

    if (adb_msg_send_lock == NULL) {
        return -1;
    }

    xSemaphoreTake(adb_msg_send_lock, portMAX_DELAY);
    if (!adb_dev_send((uint8_t *)msg, sizeof(*msg))) {
        ret = -1;
        goto done;
    }

    if (payload_size != 0U && payload != NULL) {
        adb_tx_msg((uint8_t *)payload, payload_size);
    }

done:
    xSemaphoreGive(adb_msg_send_lock);

    ADB_LOGD("adb msg send done\n");
    return ret;
}

static void adb_connect(void)
{
    static const uint8_t conn_payload[] = "device::"
                                          "ro.product.name=mido;"
                                          "ro.product.model=listenai;"
                                          "ro.product.device=mido;"
                                          "features=cmd,shell_v1";
    struct message msg = {0};

    msg.command = A_CNXN;
    msg.arg0 = A_VERSION;
    msg.arg1 = ADB_BOOT_HOST_MAX_PAYLOAD;
    adb_msg_send(&msg, conn_payload, sizeof(conn_payload));
}

static void adb_ready(uint32_t local_id, uint32_t remote_id)
{
    struct message msg = {0};

    msg.command = A_OKAY;
    msg.arg0 = local_id;
    msg.arg1 = remote_id;
    adb_msg_send(&msg, NULL, 0U);
}

void adb_close(uint32_t local_id, uint32_t remote_id)
{
    struct message msg = {0};

    adb_service_note_close_sent(local_id, remote_id);
    msg.command = A_CLSE;
    msg.arg0 = local_id;
    msg.arg1 = remote_id;
    adb_msg_send(&msg, NULL, 0U);
}

void adb_write(uint32_t local_id, uint32_t remote_id, uint8_t *data, uint32_t len)
{
    struct message msg = {0};

    msg.command = A_WRTE;
    msg.arg0 = local_id;
    msg.arg1 = remote_id;
    adb_msg_send(&msg, data, len);
}

adb_packet_t *adb_packet_alloc(uint32_t payload_len)
{
    size_t packet_size;
    adb_packet_t *packet = NULL;

#ifdef CONFIG_BOOT_ADB
    if (payload_len > MAX_PAYLOAD) {
        return NULL;
    }
#endif

    packet_size = sizeof(adb_packet_t) + payload_len;

#ifdef CONFIG_BOOT_ADB
    packet = (adb_packet_t *)adb_boot_try_inram_malloc(ADB_PACKET_ALIGN, packet_size);
#else
    packet = (adb_packet_t *)ADB_MALLOC(packet_size);
#endif

    return packet;
}

void adb_packet_free(adb_packet_t *packet)
{
    if (packet == NULL) {
        return;
    }

#ifdef CONFIG_BOOT_ADB
    inram_free(packet);
    adb_dev_notify_packet_free();
    return;
#endif

    ADB_FREE(packet);
}

static void adb_log_remote_features(const adb_packet_t *packet)
{
    const char *features;
    uint32_t len;

    if (packet->msg.data_length == 0U) {
        return;
    }

    features = strstr((const char *)packet->data, "features=");
    if (features == NULL) {
        return;
    }

    len = packet->msg.data_length - (uint32_t)(features - (const char *)packet->data);
    ADB_LOGI("remote features: %.*s\n", (int)len, features);
}

static void adb_packet_received_cb(adb_packet_t *packet)
{
    uint8_t *cmd;

    if (packet == NULL) {
        return;
    }

    cmd = (uint8_t *)&packet->msg.command;
    ADB_LOGD("adb packet recv, cmd: %c%c%c%c, payload len:%lu\n",
             cmd[0], cmd[1], cmd[2], cmd[3], (unsigned long)packet->msg.data_length);

    switch (packet->msg.command) {
    case A_CNXN:
        ADB_LOGI("adb connecting, remote info, version:%lx, max payload:%lu\n",
                 (unsigned long)packet->msg.arg0,
                 (unsigned long)packet->msg.arg1);
        adb_remote_max_payload = packet->msg.arg1;
        if (adb_remote_max_payload > MAX_PAYLOAD) {
            adb_remote_max_payload = MAX_PAYLOAD;
        }
        adb_log_remote_features(packet);
        adb_connect();
        adb_packet_free(packet);
        break;
    case A_OPEN: {
        uint32_t remote_id = packet->msg.arg0;
        uint8_t *name = packet->data;
        uint8_t *pos;
        uint32_t local_id;

        ADB_LOGI("adb open, remote_id:%lu, name:%s\n",
                 (unsigned long)remote_id, name);
        if (remote_id == 0U) {
            adb_packet_free(packet);
            break;
        }

        pos = (uint8_t *)strstr((char *)name, ":");
        if (pos == NULL) {
            ADB_LOGE("invalid name:%s\n", name);
            adb_close(0U, remote_id);
            adb_packet_free(packet);
            break;
        }

        *pos = '\0';
        local_id = adb_service_open(name, pos + 1U, remote_id);
        ADB_LOGI("adb open, local_id:%lu\n", (unsigned long)local_id);
        if (local_id != 0U) {
            adb_ready(local_id, remote_id);
        } else {
            adb_close(0U, remote_id);
        }
        adb_packet_free(packet);
        break;
    }
    case A_OKAY:
        adb_service_ready(packet->msg.arg1, packet->msg.arg0);
        adb_packet_free(packet);
        break;
    case A_CLSE:
        ADB_LOGI("adb close, local_id:%lu, remote_id:%lu\n",
                 (unsigned long)packet->msg.arg1,
                 (unsigned long)packet->msg.arg0);
        adb_service_close(packet->msg.arg1, packet->msg.arg0);
        adb_packet_free(packet);
        break;
    case A_WRTE: {
        /* The service consumes (and may free) packet, including on error. */
        uint32_t local_id = packet->msg.arg1;
        uint32_t remote_id = packet->msg.arg0;

        if (local_id == 0U || remote_id == 0U) {
            adb_packet_free(packet);
            break;
        }

        if (adb_service_write(local_id, remote_id, packet) != 0) {
            adb_close(local_id, remote_id);
        } else {
            adb_ready(local_id, remote_id);
        }
        break;
    }
    default:
        adb_packet_free(packet);
        break;
    }
}

static bool adb_packet_validate(const adb_packet_t *packet)
{
    if (packet == NULL) {
        return false;
    }

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
    /*
     * Recovery ADB already rides on USB CRC and later OTA integrity checks;
     * skipping the per-packet payload checksum removes one full SRAM walk for
     * every WRTE packet on the single-core CherryUSB path.
     */
    return true;
#endif

    if (packet->msg.data_check != adb_check_calc(packet->data, (int)packet->msg.data_length)) {
        ADB_LOGE("adb packet check sum failed, cmd:%lx\n",
                 (unsigned long)packet->msg.command);
        return false;
    }

    return true;
}

static void adb_rx_packet_finish(adb_packet_t *packet)
{
    if (packet == NULL) {
        return;
    }

    if (adb_packet_validate(packet)) {
        adb_packet_received_cb(packet);
    } else {
        adb_packet_free(packet);
    }
}

void adb_reset(void)
{
    ADB_LOGI("adb reset\n");
    adb_remote_max_payload = MAX_PAYLOAD;
    adb_service_close_all();
}

void adb_init(void)
{
    adb_dev_recv_cb_set(adb_rx_packet_finish);

    if (adb_msg_send_lock != NULL) {
        return;
    }

    adb_msg_send_lock = xSemaphoreCreateMutex();
    if (adb_msg_send_lock == NULL) {
        ADB_LOGE("adb mutex create failed\n");
    }
}
