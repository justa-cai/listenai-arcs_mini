#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lwip/api.h"
#include "lwip/opt.h"
#include "lwip/sys.h"

#include "ota_config.h"
#include "ota.h"
#include "spiflash.h"
#include "wifi_ota.h"

static uint32_t ota_flash_address = 0;
static int file_length = 0;
extern void* CRYPTO0_Handler;

void wota_sever_clear(struct netconn* conn);
int receive_data(struct netconn *conn, unsigned char *buffer,
        size_t buffer_size, int *block_index)
{
    struct netbuf *buf = NULL;
    int data_len;
    int len = 0;
    static int first_header = 1;
    int total_data_len = 0;
    uint8_t *fragment_buffer = NULL;
    int ret = -1;

    while (total_data_len < buffer_size) {
        err_t err = netconn_recv(conn, &buf);
        if (err != ERR_OK)
            return -1;

        len = netbuf_len(buf);
        if (len <= 0)
            return -1;

        WOTA_LOGD("Get file len = %d", len);
        if (len > sizeof(fragment_buffer))
            len = sizeof(fragment_buffer);

        fragment_buffer = (uint8_t *)buf->p->payload;
        if (memcmp(fragment_buffer, MAGIC_STRING, MAGIC_STRING_LEN) != 0) {
            WOTA_LOGD("Error: Frame does not start with magic string.");
            WOTA_LOGD("Maybe: %s", fragment_buffer + WOTA_MSG_OFFSET);
            ret = -1;
            goto free_out;
        }
        /* Extract block index and data length from the header */
        *block_index = (fragment_buffer[MAGIC_STRING_LEN] << 8) | fragment_buffer[MAGIC_STRING_LEN + 1];
        data_len = (fragment_buffer[MAGIC_STRING_LEN + 2] << 8) | fragment_buffer[MAGIC_STRING_LEN + 3];
        if (data_len == 0) {
            if (total_data_len > 0)
                ret = total_data_len;
            else
                ret = -1;
            goto free_out;
        }

        /* Handle first header and subsequent fragments */
        if (first_header) {
            pbuf_copy_partial(buf->p, buffer, data_len, WIFI_OTA_HEAD_SIZE);
            total_data_len += data_len;
            first_header = 0;
        } else {
            pbuf_copy_partial(buf->p, buffer + total_data_len, data_len, WIFI_OTA_HEAD_SIZE);
            total_data_len += data_len;
        }

        if (buf) {
            netbuf_delete(buf);
            buf = NULL;
        }
        if (data_len < FRAGMENT_SIZE || total_data_len == buffer_size)
            break;
        send_op_cmd(conn, OP_BLOCK_RECVD, *block_index, NULL);
    }

    ret = total_data_len;
free_out:
    if (buf)
        netbuf_delete(buf);
    return ret;
}

static int start_ota(uint8_t *ota_buff, int data_len)
{
    uint8_t res = 0;
    ls_ota_header_t *header;
    const ls_ota_ver_t *version;
    ls_ota_cmd_t cmd;

    // get fist block from server
    header = (ls_ota_header_t*)ota_buff;

    // check version
    cmd.opcode = OTA_NEW_VERSION;
    memcpy(&cmd.version, &header->version, sizeof(ls_ota_ver_t));
    res = ota_process_command(&cmd);

    if(res != OTA_VERSION_CONFIRM)
    {
        WOTA_LOG("Check version failure, new version 0x%x", header->version.version);
        return -1;
    }

    // check file size, encrypt will add 0x10 addition data
    if(file_length < header->size || file_length > header->size + 0x10)
    {
        WOTA_LOG("File length wrong, file size in header %d", header->size);
        return -1;
    }

    // start OTA
    cmd.opcode = OTA_OTA_START;
    cmd.start.size = header->size;
    cmd.start.flags = header->flags;
    cmd.start.crypto_handler = CRYPTO0_Handler;
    res = ota_process_command(&cmd);

    if(res != OTA_START_CONFIRM)
    {
        WOTA_LOGD("Start OTA failure, result %d", res);
        return -2;
    }
    return 0;
}

static int write_one_block(uint8_t *ota_buff, int data_len)
{
    uint8_t res = 0;
    ls_ota_cmd_t cmd;


    cmd.opcode = OTA_WRITE_DATA;
    cmd.data.crc32 = 0; // no check crc
    cmd.data.data = ota_buff;
    cmd.data.length = data_len;

    // write data
    WOTA_LOGD("start write flash, address 0x%x", ota_flash_address);

    cmd.data.address = ota_flash_address;
    res = ota_process_command(&cmd);

    if(res != OTA_DATA_CONFIRM)
    {
        WOTA_LOGD("Write flash failure, result %d", res);
        return -1;
    }

    // get data from server
    ota_flash_address += data_len;

    return 0;
}

void show_wota_progress(int progress, int total)
{
#if WIFI_OTA_PROGRESS_ON
    int base = total / DEFAULT_PROGRESS_FRAG;
    int i;
    int cnt;
    float percentage = ((float)progress / total) * 100.0;

    if (total <= 0)
        return;
    wifi_dbg_level_set(0x3f, 0, 5);
    WOTA_LOG_FLUSH();
    cnt = (progress % base) ? (progress / base) + 1 : (progress / base);
    WOTA_PRINT("\r[Wi-Fi OTA Progress]: ");
    for (i = 0; i < cnt; i++) {
        WOTA_PRINT("#");
    }
    WOTA_PRINT(" [%.2f%%]", percentage);
    WOTA_LOG_FLUSH();
#endif
}

/* Function to handle the file reception from TCP server */
void receive_file(struct netconn* conn)
{
    uint8_t *recv_buffer = NULL;
    int block_index = 0;
    int data_len;
    int is_first = 1;
    int total = 0;
    uint8_t res = 0;

    recv_buffer = (uint8_t *)rtos_malloc(DEFAULT_BLOCK_SIZE);
    if (!recv_buffer) {
        WOTA_LOG("No memory for receiving data.");
        return;
    }
    while (1) {
        memset(recv_buffer, 0, DEFAULT_BLOCK_SIZE);
        data_len = receive_data(conn, recv_buffer, DEFAULT_BLOCK_SIZE, &block_index);
        if (data_len <= 0) {
                WOTA_LOGD("%s, get data = 0", __func__);
                break;
        }

        WOTA_LOGD("Received block %d, data length %d bytes", block_index, data_len);
        if(is_first)
        {
            if(start_ota(recv_buffer, data_len) != ERR_OK)
            {
                wota_sever_clear(conn);
                break;
            }
            ota_flash_address = 0;
        }

        if(write_one_block(recv_buffer, data_len) != ERR_OK)
        {
            wota_sever_clear(conn);
            break;
        }

        send_op_cmd(conn, OP_BLOCK_RECVD, block_index, NULL);

        total += data_len;
        show_wota_progress(total, file_length);
        if (data_len < DEFAULT_BLOCK_SIZE)
        {
            // end of file, check ota data
            ls_ota_cmd_t cmd;

            cmd.opcode = OTA_OTA_VERIFY;
            res = ota_process_command(&cmd);
            if(res != OTA_VERIFY_CONFIRM)
            {
                WOTA_LOGD("Verify flash failure, result %d", res);
            }
            else
            {
                WOTA_LOG("\nWrite flash finish, verify success");
            }
            break;
        }
        is_first = 0;
    }

    ota_uninitialize();
    rtos_free(recv_buffer);
    WOTA_LOGD("Totally get %d bytes", total);
}

void handle_file_info(struct netconn* conn)
{
    uint8_t *frame_buffer;
    int bytes_received;
    struct netbuf *buf = NULL;
    int date_len;
    err_t err;

    err = netconn_recv(conn, &buf);
    if (err != ERR_OK)
        return;
    bytes_received = netbuf_len(buf);
    if (bytes_received <= 0) {
        WOTA_LOGD("Failed to receive response.");
        return;
    }

    if (bytes_received > 64)
        bytes_received = 64;

    frame_buffer = (uint8_t *)buf->p->payload;
    if (memcmp(frame_buffer, MAGIC_STRING, MAGIC_STRING_LEN) != 0) {
        WOTA_LOGD("Error: Invalid magic string.");
        goto free_out;
    }

    file_length = (frame_buffer[WIFI_OTA_HEAD_SIZE] << 24) |
                      (frame_buffer[WIFI_OTA_HEAD_SIZE + 1] << 16) |
                      (frame_buffer[WIFI_OTA_HEAD_SIZE + 2] << 8) |
                      frame_buffer[WIFI_OTA_HEAD_SIZE + 3];
    WOTA_LOG("File length: %d bytes", file_length);

#if WIFI_OTA_DBG
    WOTA_LOGD("First 20 bytes: ");
    for (int i = 0; i < 20 && WIFI_OTA_HEAD_SIZE + 4 + i < bytes_received; i++) {
        WOTA_PRINT("%02X ", (unsigned char)frame_buffer[WIFI_OTA_HEAD_SIZE + 4 + i]);
    }
    WOTA_LOGD("");
#endif

free_out:
    if (buf)
        netbuf_delete(buf);
}

/* Function to send operation commands to the server */
void send_op_cmd(struct netconn* conn, unsigned short op_cmd,
        unsigned short block_size, const char* filename)
{
    unsigned char buffer[64] = {0};
    unsigned short filename_len = 0;

    if (filename)
        filename_len = strlen(filename);
    /* Pack the command */
    buffer[0] = (op_cmd >> 8) & 0xFF;
    buffer[1] = op_cmd & 0xFF;
    buffer[2] = (block_size >> 8) & 0xFF;
    buffer[3] = block_size & 0xFF;
    buffer[4] = (filename_len >> 8) & 0xFF;
    buffer[5] = filename_len & 0xFF;
    if (filename)
        memcpy(buffer + 6, filename, filename_len);

    /* Send the command to the server */
    netconn_write(conn, buffer, 6 + filename_len, NETCONN_COPY);
}

void wota_sever_clear(struct netconn* conn)
{
    struct netbuf *buf = NULL;

    send_op_cmd(conn, OP_WOTA_ABORT, 0, NULL);
    err_t err = netconn_recv(conn, &buf);
    if (err != ERR_OK)
        return;
    netbuf_delete(buf);
}

FLASH_DEV ota_flash = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF, //divider is 1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000,
    .addr_bytes = 3,
    .addr_auto = 0,
};

int wifi_ota_start(const uint8_t *file_name, const uint8_t *ip_str, uint16_t port, void *extra)
{
    struct netconn* conn;
    err_t err;
	ip_addr_t ip_addr = {0};

    if (!file_name || !strlen(file_name))
        file_name = DEF_FW_NAME;

    if (!ip_str || !strlen(ip_str))
        ip_str = DEFAULT_IP;

    if (!port)
        port = DEFAULT_PORT;

    /// initialize flash driver
    err = flash_init(&ota_flash, 0, 0);
    if(err != ERR_OK)
    {
        WOTA_LOG("Flash Driver Initialize error, ret=%d ", err);
        return -1;
    }
    err = ota_initialize(&ota_flash);
    if(err != OTA_SUCCESS)
    {
        WOTA_LOG("OTA Initialize error, ret=%d ", err);
        return -1;
    }

    ipaddr_aton(ip_str, &ip_addr);
    conn = netconn_new(NETCONN_TCP);
    if (conn == NULL) {
        WOTA_LOG("Failed to create connection");
        return -1;
    }

    err = netconn_connect(conn, &ip_addr, port);
    if (err != ERR_OK) {
        WOTA_LOG("Connection failed, err = %d", err);
        return -1;
    }

    /* Clear to send */
    wota_sever_clear(conn);

    send_op_cmd(conn, OP_FILE_INFO, 64, file_name);
    handle_file_info(conn);

    send_op_cmd(conn, OP_READ_FILE, DEFAULT_BLOCK_SIZE, file_name);
    receive_file(conn);

    netconn_close(conn);
    netconn_delete(conn);

    return 0;
}
