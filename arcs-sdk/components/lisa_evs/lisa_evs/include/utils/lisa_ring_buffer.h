#ifndef _KFIFO_HEADER_H_
#define _KFIFO_HEADER_H_

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//判断x是否是2的次方
#define is_power_of_2(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))
//取a和b中最小值
#define min(a, b) (((a) < (b)) ? (a) : (b))

struct lisa_ring_buffer
{
	uint8_t *buffer; //缓冲区
	unsigned int size; //大小
	unsigned int in; //入口位置
	unsigned int out; //出口位置
};

//初始化缓冲区
struct lisa_ring_buffer* lisa_ring_buffer_init(uint8_t *buffer, unsigned int size);

//释放缓冲区
void lisa_ring_buffer_free(struct lisa_ring_buffer *ring_buf);

unsigned int lisa_ring_buffer_len(const struct lisa_ring_buffer *ring_buf);

unsigned int lisa_ring_buffer_get(struct lisa_ring_buffer *ring_buf, uint8_t *buffer, unsigned int size);

unsigned int lisa_ring_buffer_put(struct lisa_ring_buffer *ring_buf, uint8_t *buffer, unsigned int size);
#endif //_KFIFO_HEADER_H_

