#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils/lisa_ring_buffer.h"
#include "lisa_mem.h"
#include "lisa_log.h"

#define TAG "ring_buffer"

//初始化缓冲区
struct lisa_ring_buffer* lisa_ring_buffer_init(uint8_t *buffer, unsigned int size)
{
	struct lisa_ring_buffer *ring_buf = NULL;
	if (!buffer)
		return ring_buf;
	if (!is_power_of_2(size)) {
		LISA_LOGE(TAG, "size must be power of 2.");
		return ring_buf;
	}
	ring_buf = (struct lisa_ring_buffer *)lisa_mem_alloc(sizeof(struct lisa_ring_buffer));
    if (!ring_buf) {
		LISA_LOGE(TAG, "Failed to psram_malloc memory");
		return ring_buf;
	}
	memset(ring_buf, 0, sizeof(struct lisa_ring_buffer));
	ring_buf->buffer = buffer;
	ring_buf->size = size;
	ring_buf->in = 0;
	ring_buf->out = 0;
	return ring_buf;
}

//释放缓冲区
void lisa_ring_buffer_free(struct lisa_ring_buffer *ring_buf)
{
    if (ring_buf) {
	    if (ring_buf->buffer) {
			  lisa_mem_free(ring_buf->buffer);
        ring_buf->buffer = NULL;
	    }
	    lisa_mem_free(ring_buf);
	    ring_buf = NULL;
	}
}

//缓冲区的长度
unsigned int __listenai_ring_buffer_len(const struct lisa_ring_buffer *ring_buf)
{
    return (ring_buf->in - ring_buf->out);
}

//从缓冲区中取数据
static unsigned int __listenai_ring_buffer_get(struct lisa_ring_buffer *ring_buf, unsigned char * buffer, unsigned int size)
{
	if (!ring_buf || !buffer)
		return 0;
    unsigned int len = 0;
    size  = min(size, ring_buf->in - ring_buf->out);
    /* first get the data from fifo->out until the end of the buffer */
    len = min(size, ring_buf->size - (ring_buf->out & (ring_buf->size - 1)));
    memcpy(buffer, ring_buf->buffer + (ring_buf->out & (ring_buf->size - 1)), len);
    /* then get the rest (if any) from the beginning of the buffer */
    memcpy(buffer + len, ring_buf->buffer, size - len);
    ring_buf->out += size;
    return size;
}

//向缓冲区中存放数据
unsigned int lisa_ring_buffer_put(struct lisa_ring_buffer *ring_buf, uint8_t *buffer, unsigned int size)
{
	if (!ring_buf || !buffer)
		return 0;
    unsigned int len = 0;
	size = min(size, ring_buf->size - ring_buf->in + ring_buf->out);
    /* first put the data starting from fifo->in to buffer end */
    len = min(size, ring_buf->size - (ring_buf->in & (ring_buf->size - 1)));

    memcpy(ring_buf->buffer + (ring_buf->in & (ring_buf->size - 1)), buffer, len);
    /* then put the rest (if any) at the beginning of the buffer */
    memcpy(ring_buf->buffer, buffer + len, size - len);
    ring_buf->in += size;
    return size;
}

unsigned int lisa_ring_buffer_len(const struct lisa_ring_buffer *ring_buf)
{
    unsigned int len = 0;
    len = __listenai_ring_buffer_len(ring_buf);
    return len;
}

unsigned int lisa_ring_buffer_get(struct lisa_ring_buffer *ring_buf, unsigned char *buffer, unsigned int size)
{
    unsigned int ret;
    ret = __listenai_ring_buffer_get(ring_buf, buffer, size);
    //buffer中没有数据
    if (ring_buf->in == ring_buf->out)
      ring_buf->in = ring_buf->out = 0;
    return ret;
}
