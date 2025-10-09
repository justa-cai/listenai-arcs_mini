#include "stdint.h"
#include "disk/disk_access.h"
#include "ic_message.h"
#include "platform.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"


#include "lib_sdc.h"
#include "drv_sdc.h"

#include "disk_mem.h"
#include "log_print.h"


#if CONFIG_DISK_MEMORY_DEBUG_CRC
#include "../crc32/crc32.h"
#endif

#define DISK_MEM_MSG_OPCODE_REQUEST  (0)
#define DISK_MEM_MSG_OPCODE_RESPONSE (1)

#define DISK_MEM_PSRAM_2_URPC_VALUE(addr) ((addr)- CONFIG_DISK_MEM_PSRAM_BASE)
#define DISK_MEM_URPC_2_PSRAM_VALUE(addr) ((addr) + CONFIG_DISK_MEM_PSRAM_BASE)
#define DISK_MEM2SEC(addr, sec_size) ((addr) / (sec_size))
#define DISK_MEM_MSGQ_COUNT 10
enum {
	DISK_MEM_RES_ERR_CODE_NONE = 0,
	DISK_MEM_RES_ERR_CODE_FAILED = 1,
};

enum {
	DISK_MEM_REQ_NORMAL = 0,
	DISK_MEM_REQ_IMMEDIATELY = 1,
};

typedef struct disk_mem_msg {
	uint32_t opcode : 1;	 /* 0: request, 1:responese */
	uint32_t ext : 1;	 /* err code for responese, flag for request */
	uint32_t nand_addr : 30; /* max support 1024M */
	uint32_t buf : 24;	 /* max support 16M */
	uint32_t size : 24;	 /* max support 16M */
} __attribute__((packed)) disk_mem_msg_t;

typedef struct disk_mem_context {
	const void *disk_drv;
	uint32_t sec_cnt;
	uint32_t sec_size;

} disk_mem_context_t;

disk_mem_context_t disk_mem_ctx;
static QueueHandle_t disk_mem_msgq;

static int disk_mem_init(void)
{
	int err;
	uint32_t blk_len, blk_num, erase_size;

	disk_mem_ctx.disk_drv = CONFIG_DISK_SDMMC_VOLUME_NAME;

    err = lib_sdc_get_card_info(CONFIG_DISK_MEM_SD_INDEX, &blk_len, &blk_num, &erase_size);
    if(err != 0){
		CLOGE("disk init failed, err:%d", err);
        return err;
    }
	disk_mem_ctx.sec_cnt = blk_num;
	disk_mem_ctx.sec_size = blk_len;
	
	CLOGI("disk init succeeded, sector count:%d, sector size:%d", disk_mem_ctx.sec_cnt,
		disk_mem_ctx.sec_size);

	return 0;
}


static void disk_mem_urpc_evt_handler(uint8_t *frame)
{
	disk_mem_msg_t *msg = (disk_mem_msg_t *)frame;

	CLOGD("disk mem request received, nand:0x%x, buf:0x%x, size:%d", msg->nand_addr,
		DISK_MEM_URPC_2_PSRAM_VALUE(msg->buf), msg->size);

	if (msg->ext == DISK_MEM_REQ_IMMEDIATELY) {
		CLOGE("Unsupported DISK_MEM_REQ_IMMEDIATELY request");
	} else {
		if (msg->buf == 0x00 && msg->nand_addr == 0x00) {
			CLOGE("Invailed request address");
		} else {
			int err = xQueueSend(disk_mem_msgq, msg, portMAX_DELAY);
			if (err != pdTRUE) {
				CLOGE("disk mem msgq put failed, err:%d", err);
			}
		}
	}
}

static int32_t disk_mem_ic_message_handler(ic_message_handle_info_t *handle_info, ic_message_msg_info_t* msg_info)
{
	(void)handle_info;

	disk_mem_urpc_evt_handler((uint8_t *)(msg_info->msg));

	return 0;
}


static inline int disk_memcpy(void *dst, void *src, uint32_t size)
{
	int err;
	uint8_t* data = dst;
	uint32_t len;

	len = (size + disk_mem_ctx.sec_size - 1) / disk_mem_ctx.sec_size * disk_mem_ctx.sec_size;

	err = gm_sdc_api_sdcard_sector_read(
		CONFIG_DISK_MEM_SD_INDEX, 
		DISK_MEM2SEC((uint32_t)src,disk_mem_ctx.sec_size), 
		DISK_MEM2SEC((uint32_t)len,disk_mem_ctx.sec_size), 
		dst);
	return err;
}

static void disk_mem_urpc_notify_done(disk_mem_msg_t *msg)
{
	int err = ic_message_msg_send_by_id(IC_MESSAGE_ID_DISK_MEM, IC_MESSAGE_MSG_TYPE_CMD, (uint8_t *)msg, sizeof(disk_mem_msg_t));
	if (err != IC_MESSAGE_ERR_NONE) {
		CLOGE("disk mem send notify failed: %d", err);
	}
}

static void disk_mem_server_thread_handler(void *p1)
{
	int err;
	disk_mem_msg_t msg;
	TickType_t ticks;
	while (1) {

		err = xQueueReceive(disk_mem_msgq, &msg, portMAX_DELAY);
		if ((err != pdTRUE)||(msg.buf == 0)) {
			continue;
		}

		ticks = xTaskGetTickCount();
		err = disk_memcpy((void *)DISK_MEM_URPC_2_PSRAM_VALUE(msg.buf),
				  (void *)((uint32_t)msg.nand_addr), msg.size);

		ticks = xTaskGetTickCount() - ticks;

		msg.opcode = DISK_MEM_MSG_OPCODE_RESPONSE;
		msg.ext = !!err;

		DISK_MEM_CACHE_CLEAN((void *)DISK_MEM_URPC_2_PSRAM_VALUE(msg.buf), msg.size);
#if CONFIG_DISK_MEMORY_DEBUG_CRC
		uint32_t crc32 = crc32_calc(DISK_MEM_URPC_2_PSRAM_VALUE(msg.buf), msg.size, 0);
#endif

		CLOGD("disk memcpy done, err:%d, nand:0x%x, buf:0x%x, size:%d, time:%lld ms\n", 
			err,
			msg.nand_addr, 
			DISK_MEM_URPC_2_PSRAM_VALUE(msg.buf), 
			msg.size, 
			(ticks * (1000 / configTICK_RATE_HZ)));
#if CONFIG_DISK_MEMORY_DEBUG_CRC
		CLOGI("crc32:0x%x", crc32);
#endif
		
		disk_mem_urpc_notify_done(&msg);
		
	}
}

#define DISK_MEM_SERVER_THREAD_STACK_SIZE CONFIG_DISK_MEM_THREAD_STACK_SIZE
#define DISK_MEM_THREAD_PRIORITY CONFIG_DISK_MEM_THREAD_PRIORITY
void disk_mem_urpc_init(void)
{
	int err;

	disk_mem_msgq = xQueueCreate(DISK_MEM_MSGQ_COUNT, sizeof(disk_mem_msg_t));
	disk_mem_init();
	ic_message_register_by_id(IC_MESSAGE_ID_DISK_MEM, disk_mem_ic_message_handler, NULL);
	
    err = xTaskCreate(
        disk_mem_server_thread_handler, 
        "disk_mem_server_thread", 
        DISK_MEM_SERVER_THREAD_STACK_SIZE, 
        NULL, 
        DISK_MEM_THREAD_PRIORITY, 
        NULL);

	CLOGI("disk mem inited");
}
