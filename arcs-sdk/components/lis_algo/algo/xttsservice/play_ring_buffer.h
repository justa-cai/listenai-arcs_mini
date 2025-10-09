#ifndef _play_ring_buffer_HEADER_H_
#define _play_ring_buffer_HEADER_H_

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "event_groups.h"


// 缓冲区大小

//判断x是否是2的次方
#define is_power_of_2(x) ((x) != 0 && (((x) & ((x)-1)) == 0))
//取a和b中最小值
#define min(a, b) (((a) < (b)) ? (a) : (b))
// 阻塞ringbuffer使能
#define RING_BUFFER_BLOCK_ENABLE 0


#define RINGBUFF_CAN_PUT  (1<<0)
#define RINGBUFF_CAN_GET  (1<<1)

struct play_ring_buffer
{
	uint8_t *buffer;   //缓冲区
	unsigned int size; //大小
	unsigned int in;   //入口位置
	unsigned int out;  //出口位置
#if RING_BUFFER_BLOCK_ENABLE
	EventGroupHandle_t ringbuff_sta;
#endif
};

//初始化缓冲区
struct play_ring_buffer *play_ring_buffer_init(struct play_ring_buffer *ring_buf, uint8_t *buffer, unsigned int size);

//释放缓冲区
void play_ring_buffer_free(struct play_ring_buffer *ring_buf);

void play_ring_buffer_reset(struct play_ring_buffer *ring_buf);

unsigned int play_ring_buffer_len(const struct play_ring_buffer *ring_buf);

unsigned int play_ring_buffer_get(struct play_ring_buffer *ring_buf, uint8_t *buffer, unsigned int size);

unsigned int play_ring_buffer_put(struct play_ring_buffer *ring_buf, uint8_t *buffer, unsigned int size);
unsigned int play_ring_buffer_avail(const struct play_ring_buffer *ring_buf);

unsigned int play_ring_buffer_get_block(struct play_ring_buffer *ring_buf, uint8_t *buffer, unsigned int size, int wait_timeout);
unsigned int play_ring_buffer_put_block(struct play_ring_buffer *ring_buf, uint8_t *buffer, unsigned int size, int wait_timeout);
#endif
