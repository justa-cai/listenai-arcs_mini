#ifndef __DISK_MEM_H__
#define __DISK_MEM_H__

#define DISK_MEM_TEST 0

#if DISK_MEM_TEST == 1
#define DISK_MEM_TEST_PSRAM_MAX_SIZE_MB (6)
#define DISK_MEM_TEST_RUNTIME_CRC 0
#endif

#define DISK_MEM_RESERVED_FLAG_IMMEDIATELY (1 << 0)

typedef struct disk_mem_msg {
	uint32_t opcode : 1; /* 0: request, 1:responese */
	uint32_t err : 1;
	uint32_t reserved : 30;
	uint32_t nand_addr; /* max support 1024M */
	uint32_t buf; /* max support 16M */
	uint32_t size; /* max support 16M */
} __attribute__((packed)) disk_mem_msg_t;

typedef void (*disk_mem_callback_t)(disk_mem_msg_t *, void *);

/* Make sure don't touch the buf address before the callback */
int32_t disk_mem_request(disk_mem_msg_t *msg, disk_mem_callback_t cb, void *datas);

int32_t disk_mem_init(void);

int32_t disk_mem_read_request(void *dst, void *src, uint32_t size, disk_mem_callback_t cb,
			      void *user_data);

int32_t disk_mem_read_request_sync(void *dst, void *src, uint32_t size, void *user_data);

int32_t disk_mem_read_request_async(void *dst, void *src, uint32_t size, void *user_data);

int32_t disk_mem_read_request_async_wait(uint32_t timeout_ms);

#endif
