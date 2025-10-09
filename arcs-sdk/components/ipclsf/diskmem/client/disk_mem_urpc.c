#include <stdint.h>
#include <string.h>
#include "disk_mem.h"
#include "ic_message.h"
#include "platform.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "disk_mem.h"
#include "log_print.h"
#include "lisa_log.h"

#define DISK_MEM_CALLBACK_COUNT 10
#define DISK_MEM_RESP_COUNT 3

// 缓存清理分段配置
#define DCACHE_CHUNK_SIZE (128 << 10)  // 每次清理128KB

#define DISK_MEM_PSRAM_2_URPC_VALUE(addr) ((addr) - CONFIG_DISK_MEM_PSRAM_BASE)
#define DISK_MEM_URPC_2_PSRAM_VALUE(addr) ((addr) + CONFIG_DISK_MEM_PSRAM_BASE)

typedef struct disk_mem_urpc_msg {
	uint32_t opcode : 1; /* 0: request, 1:responese */
	uint32_t ext : 1; /* 0: ok, 1:err for responese, 0: normal 1: immediately of request*/
	uint32_t nand_addr : 30; /* max support 1024M */
	uint32_t buf : 24; /* max support 16M */
	uint32_t size : 24; /* max support 16M */
} __attribute__((packed)) disk_mem_urpc_msg_t;

typedef struct callback_info {
	uint32_t nand;
	uint32_t size;
	disk_mem_callback_t cb;
	void *datas;
} callback_info_t;

typedef struct{
	uint32_t err_code;/*0:拷贝成功，其他:拷贝失败*/
}disk_mem_copy_resp_t;

#define INVALID_NAND_ADDR (0xFFFFFFFF)

typedef struct disk_mem_msgq_item {
	disk_mem_urpc_msg_t msg;
} __attribute__((aligned(4))) disk_mem_msgq_item_t;

static callback_info_t callbacks[DISK_MEM_CALLBACK_COUNT] = { 0 };
static callback_info_t callbacks_break[DISK_MEM_CALLBACK_COUNT] = { 0 };
static QueueHandle_t disk_mem_msgq;
static QueueHandle_t rx_queue;

#define DISK_MEM_THREAD_STACK_SIZE CONFIG_DISK_MEM_THREAD_STACK_SIZE
#define DISK_MEM_THREAD_PRIORITY CONFIG_DISK_MEM_THREAD_PRIORITY

int32_t disk_mem_handler(ic_message_handle_info_t *header, ic_message_msg_info_t* body)
{
	uint8_t *frame = (uint8_t *)body->msg;
	disk_mem_msgq_item_t item;
	/* urpc payload size is 10 byte */
	memcpy(&item.msg, &frame[0], 10);
	xQueueSend(disk_mem_msgq, &item, portMAX_DELAY);
	return 0;
}

static void disk_mem_cb(disk_mem_msg_t *msg, void *user_data){
    disk_mem_copy_resp_t resp;
    resp.err_code = msg->err;

    if (pdTRUE != xQueueSend(rx_queue, &resp, 0)){
        LOGE("Drop message");
    }
}

/**
 * @brief 分段缓存清理函数，确保32字节对齐
 * @param start 起始地址
 * @param end 结束地址
 */
static void disk_mem_dcache_invalidate_chunked(unsigned long start, unsigned long end)
{
    if (start >= end) {
        return;
    }

    unsigned long total_size = end - start;
    unsigned long processed = 0;
    
    while (processed < total_size) {
        unsigned long chunk_start = start + processed;
        unsigned long chunk_size = (total_size - processed > DCACHE_CHUNK_SIZE) ? 
                                  DCACHE_CHUNK_SIZE : (total_size - processed);
        unsigned long chunk_end = chunk_start + chunk_size;
        
        dcache_invalidate_range(chunk_start, chunk_end);
        
        processed += chunk_size;
    }
}

// static void disk_mem_dcache_clean_chunked(unsigned long start, unsigned long end)
// {
//     if (start >= end) {
//         return;
//     }

//     unsigned long total_size = end - start;
//     unsigned long processed = 0;
    
//     while (processed < total_size) {
//         unsigned long chunk_start = start + processed;
//         unsigned long chunk_size = (total_size - processed > DCACHE_CHUNK_SIZE) ? 
//                                   DCACHE_CHUNK_SIZE : (total_size - processed);
//         unsigned long chunk_end = chunk_start + chunk_size;
        
//         dcache_clean_range(chunk_start, chunk_end);
        
//         processed += chunk_size;
//     }
// }


static void disk_mem_thread(void * arg)
{
	disk_mem_msgq_item_t item;
	uint8_t i;
	callback_info_t *cb_info = NULL;
	while (1) {
		int is_found = 0;
		xQueueReceive(disk_mem_msgq, &item, portMAX_DELAY);
		// CLOGD("disk mem request done, err:%d, buf:0x%x, nand:0x%x, size:%d", item.msg.ext,
		//       item.msg.buf, item.msg.nand_addr, item.msg.size);

		cb_info = (item.msg.ext == 1) ? callbacks_break : callbacks;

		for (i = 0; i < DISK_MEM_CALLBACK_COUNT; i++) {
			if (cb_info[i].size == item.msg.size) {
				if (cb_info[i].cb != NULL) {
					disk_mem_msg_t msg = { 0 };
					msg.buf = DISK_MEM_URPC_2_PSRAM_VALUE(item.msg.buf);
					msg.err = item.msg.ext;
					msg.nand_addr = item.msg.nand_addr;
					msg.size = item.msg.size;
					msg.opcode = item.msg.opcode;
					
					if((msg.buf != CONFIG_DISK_MEM_PSRAM_BASE)&&(msg.size != 0)){
						// DISK_MEM_CACHE_INVALID((uint32_t *)(uintptr_t)msg.buf, msg.size);
						disk_mem_dcache_invalidate_chunked((unsigned long)msg.buf, (unsigned long)(msg.buf + msg.size));
					}
					
#if DISK_MEM_TEST_RUNTIME_CRC == 1
					crc32_init(0);
					uint32_t crc = crc32_calc(msg.buf, msg.size, 0);
					LOGI("buf:%x, size:%d, crc value:%x", msg.buf, msg.size,
					      crc);
#endif
					// __disable_irq();
					disk_mem_callback_t cb_cb = cb_info[i].cb;
					// cb_info[i].cb(&msg, cb_info[i].datas);
					is_found = 1;
				

					cb_info[i].nand = INVALID_NAND_ADDR;
					cb_info[i].cb = 0;
					cb_info[i].size = 0;
					cb_cb(&msg, cb_info[i].datas);
					// __enable_irq();
					// (void)ps;
					break;
				}
			}
		}

		if((is_found == 0)&&(!((item.msg.ext == 0)&&(item.msg.size)))){
			LOGW("Can't find %s,size(%d),ext(%d),ch_info:", 
					(item.msg.ext == 1) ? "callbacks_break" : "callbacks",
					item.msg.size,
					item.msg.ext);
			for (i = 0; i < DISK_MEM_CALLBACK_COUNT; i++) {
				LOGD("i:%d,size:%d,cb:%p",i,cb_info[i].size,cb_info[i].cb);
			}

		}

		/*如果资源加载被打断，仍然调用资源加载的cb，避免加载阻塞*/
		if(item.msg.ext == 1){
			is_found = 0;
			cb_info = callbacks;

			for (i = 0; i < DISK_MEM_CALLBACK_COUNT; i++) {
				if (cb_info[i].size == item.msg.size) {
					if (cb_info[i].cb != NULL) {
						disk_mem_msg_t msg = { 0 };
						msg.buf = DISK_MEM_URPC_2_PSRAM_VALUE(item.msg.buf);
						msg.err = item.msg.ext;
						msg.nand_addr = item.msg.nand_addr;
						msg.size = item.msg.size;
						msg.opcode = item.msg.opcode;
						
						if((msg.buf != CONFIG_DISK_MEM_PSRAM_BASE)&&(msg.size != 0)){
							// DISK_MEM_CACHE_INVALID((uint32_t *)(uintptr_t)msg.buf, msg.size);
							disk_mem_dcache_invalidate_chunked((unsigned long)msg.buf, (unsigned long)(msg.buf + msg.size));
						}
						
	#if DISK_MEM_TEST_RUNTIME_CRC == 1
						crc32_init(0);
						uint32_t crc = crc32_calc(msg.buf, msg.size, 0);
						LOGI("buf:%x, size:%d, crc value:%x", msg.buf, msg.size,
							crc);
	#endif
						cb_info[i].cb(&msg, cb_info[i].datas);
						is_found = 1;
					

						// __disable_irq();
						cb_info[i].nand = INVALID_NAND_ADDR;
						cb_info[i].cb = 0;
						cb_info[i].size = 0;
						// __enable_irq();
						// (void)ps;
						break;
					}
				}
			}

			if(is_found == 0){
				LOGW("Can't find callback,size(%d) by break request,ch_info:", item.msg.size);
				for (i = 0; i < DISK_MEM_CALLBACK_COUNT; i++) {
					LOGD("i:%d,size:%d,cb:%p",i,cb_info[i].size,cb_info[i].cb);
				}
			}		
		}
	}
}

int32_t disk_mem_init(void)
{
	int err;
	uint32_t i;

	err = ic_message_register_by_id(IC_MESSAGE_ID_DISK_MEM, disk_mem_handler,NULL);
	if (err != 0) {
		LOGE("disk mem service register failed, err:%d", err);
		return err;
	}
	disk_mem_msgq = xQueueCreate(DISK_MEM_CALLBACK_COUNT, sizeof(disk_mem_msgq_item_t));
	if (disk_mem_msgq == NULL) {
		LOGE("disk mem create msgq failed, size:%d",sizeof(disk_mem_msgq_item_t));
		return -1;
	}

	rx_queue = xQueueCreate(DISK_MEM_RESP_COUNT, sizeof(disk_mem_copy_resp_t));
	if (rx_queue == NULL) {
		LOGE("disk mem create rx_queue failed, size:%d",sizeof(disk_mem_copy_resp_t));
		return -1;
	}

    err = xTaskCreate(
        disk_mem_thread, 
        "disk_mem_thread", 
        DISK_MEM_THREAD_STACK_SIZE, 
        NULL, 
        DISK_MEM_THREAD_PRIORITY, 
        NULL);

	if (err != pdTRUE) {
		LOGE("disk mem create task failed, err:%d", err);
		return err;
	}

	
	for (i = 0; i < DISK_MEM_CALLBACK_COUNT; i++) {
		callbacks[i].nand = INVALID_NAND_ADDR;
		callbacks[i].cb = 0;
	}

#if DISK_MEM_TEST == 1
	extern void disk_mem_request_run(void);
	disk_mem_request_run();
#endif

	return 0;
}

int32_t disk_mem_request(disk_mem_msg_t *msg, disk_mem_callback_t cb, void *user_data)
{
	uint8_t i = 0;
	int err;

	if (msg == NULL || msg->nand_addr == INVALID_NAND_ADDR) {
		return -1;
	}

	if (msg->size > (16 * 1024 * 1024)) {
		return -2;
	}

	if (DISK_MEM_PSRAM_2_URPC_VALUE(msg->buf) > (16 * 1024 * 1024)) {
		return -3;
	}

	if (msg->nand_addr > (1024 * 1024 * 1024)) {
		return -4;
	}

	if (msg->reserved & DISK_MEM_RESERVED_FLAG_IMMEDIATELY) {
		/* clear all callbacks */
		for (i = 0; i < DISK_MEM_CALLBACK_COUNT; i++) {
			// __disable_irq();
			callbacks[i].nand = INVALID_NAND_ADDR;
			callbacks[i].size = 0;
			callbacks[i].cb = 0;
			callbacks[i].datas = NULL;
			// __enable_irq();
			// (void)ps;
		}
	}

	if(msg->buf != 0x60000000){
		for (i = 0; i < DISK_MEM_CALLBACK_COUNT; i++) {
			if (callbacks[i].size == 0 || callbacks[i].size == msg->size) {
				// __disable_irq();
				callbacks[i].nand = msg->nand_addr;
				callbacks[i].cb = cb;
				callbacks[i].datas = user_data;
				callbacks[i].size = msg->size;
				// __enable_irq();
				// (void)ps;
				break;
			}
		}

		if (i >= DISK_MEM_CALLBACK_COUNT) {
			/* no more free space */
			return -5;
		}
	} else {
		if(cb != NULL){
			for (i = 0; i < DISK_MEM_CALLBACK_COUNT; i++) {
				if (callbacks_break[i].cb == NULL || callbacks_break[i].size == msg->size) {
					// __disable_irq();
					callbacks_break[i].nand = msg->nand_addr;
					callbacks_break[i].cb = cb;
					callbacks_break[i].datas = user_data;
					callbacks_break[i].size = msg->size;
					// __enable_irq();
					// (void)ps;
					LOGI("Add callbacks_break cb:%p size:%d", callbacks_break[i].cb, callbacks_break[i].size );

					break;
				}
			}
		}
		else{
			LOGW("cb is NULL not add callback(%p) for size:%d", cb,callbacks_break[i].size );

		}

	}

	disk_mem_urpc_msg_t urpc_msg = { 0 };

	urpc_msg.buf = DISK_MEM_PSRAM_2_URPC_VALUE(msg->buf);
	urpc_msg.ext = msg->reserved & DISK_MEM_RESERVED_FLAG_IMMEDIATELY;
	urpc_msg.opcode = 0;
	urpc_msg.nand_addr = msg->nand_addr;
	urpc_msg.size = msg->size;

	/* Ensure that the address is cleared from the cache */
	// DISK_MEM_CACHE_CLEAN((uint32_t *)(uintptr_t)msg->buf, msg->size);
	dcache_clean_range((unsigned long)msg->buf, (unsigned long)(msg->buf + msg->size));
	// LOGD("disk mem request, buf:0x%x, nand:0x%x, size:%d", urpc_msg.buf, urpc_msg.nand_addr, urpc_msg.size);

	while(!ic_message_remote_is_connected(IC_MESSAGE_ID_DISK_MEM)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
	/* send msg to ap */
	err = ic_message_msg_send_by_id(IC_MESSAGE_ID_DISK_MEM, IC_MESSAGE_MSG_TYPE_EVT,
				       (uint8_t *)&urpc_msg, sizeof(disk_mem_urpc_msg_t));
	if (err != 0) {
		LOGE("disk mem request failed: %d", err);
		return err;
	}

	return 0;
}

int32_t disk_mem_read_request(void *dst, void *src, uint32_t size, disk_mem_callback_t cb,
			      void *user_data)
{
	disk_mem_msg_t msg = {
		.buf = (uint32_t)dst,
		.size = size,
		.opcode = 0,
		.nand_addr = (uint32_t)src,
	};

	return disk_mem_request(&msg, cb, user_data);
}

int32_t disk_mem_read_request_sync(void *dst, void *src, uint32_t size, void *user_data)
{
	int ret;
    disk_mem_copy_resp_t resp;

	ret = disk_mem_read_request(dst, src, size, disk_mem_cb, user_data);

	if(pdTRUE != xQueueReceive(rx_queue, &resp, pdMS_TO_TICKS(10000))){
		LOGE("disk_mem_read_request_sync timeout ret:%d!", ret);
	}

	return ret;
}

int32_t disk_mem_read_request_async(void *dst, void *src, uint32_t size, void *user_data)
{
	return disk_mem_read_request(dst, src, size, disk_mem_cb, user_data);
}

int32_t disk_mem_read_request_async_wait(uint32_t timeout_ms)
{
	disk_mem_copy_resp_t resp;
	if(pdTRUE != xQueueReceive(rx_queue, &resp, pdMS_TO_TICKS(timeout_ms))){
		LOGE("disk_mem_read_request_sync timeout!");
		return -5;
	}
	return 0;
}

int32_t disk_mem_clear_request(disk_mem_callback_t cb, void *user_data)
{
	disk_mem_msg_t msg = {
		.buf = (uint32_t)0x60000000, /*  */
		.size = 0,
		.opcode = 0,
		.reserved = DISK_MEM_RESERVED_FLAG_IMMEDIATELY,
		.nand_addr = 0,
	};

	return disk_mem_request(&msg, cb, user_data);
}

int32_t disk_mem_read_request_stop_by_size(disk_mem_callback_t cb, void *user_data, uint32_t size)
{
	disk_mem_msg_t msg = {
		.buf = (uint32_t)0x60000000, /*  */
		.size = size,
		.opcode = 0,
		.reserved = 0,
		.nand_addr = 0,
	};

	return disk_mem_request(&msg, cb, user_data);
}
