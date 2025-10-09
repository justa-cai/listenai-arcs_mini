#include "play_ring_buffer.h"
#include "log_print.h"

//初始化缓冲区
struct play_ring_buffer *play_ring_buffer_init(struct play_ring_buffer *ring_buf, uint8_t *buffer, unsigned int size)
{
    //struct play_ring_buffer *ring_buf = NULL;
    if (!buffer)
    {
        CLOGE("buffer null");
        return NULL;
    }

    if (!is_power_of_2(size))
    {
       CLOGE("size must be power of 2.");
        return ring_buf;
    }
    
    if (!ring_buf)
    {
        ring_buf = (struct play_ring_buffer *)malloc(sizeof(struct play_ring_buffer));
        if(!ring_buf)
        {
            CLOGE("Failed to alloct memory");
            return ring_buf;
        }
    }
    memset(ring_buf, 0, sizeof(struct play_ring_buffer));
    ring_buf->buffer = buffer;
    ring_buf->size = size;
    ring_buf->in = 0;
    ring_buf->out = 0;
#if RING_BUFFER_BLOCK_ENABLE
    ring_buf->ringbuff_sta = xEventGroupCreate();
#endif
    return ring_buf;
}

//重置缓冲区
void play_ring_buffer_reset(struct play_ring_buffer *ring_buf)
{
    if (ring_buf)
    {
        if (ring_buf->buffer)
        {
            //memset(ring_buf->buffer, 0, ring_buf->size);
        }
        ring_buf->in = 0;
        ring_buf->out = 0;
#if RING_BUFFER_BLOCK_ENABLE
        vEventGroupDelete(ring_buf->ringbuff_sta);
        ring_buf->ringbuff_sta = xEventGroupCreate();
#endif
    }
}

//释放缓冲区
void play_ring_buffer_free(struct play_ring_buffer *ring_buf)
{
    if (ring_buf)
    {
        if(ring_buf->buffer)
        {
            free(ring_buf->buffer);
        }
        free(ring_buf);
        ring_buf = NULL;
    }
}

//缓冲区的长度
unsigned int __play_ring_buffer_len(const struct play_ring_buffer *ring_buf)
{
    return (ring_buf->in - ring_buf->out);
}

//从缓冲区中取数据
static unsigned int __play_ring_buffer_get(struct play_ring_buffer *ring_buf, unsigned char *buffer, unsigned int size)
{
    if (!ring_buf || !buffer)
        return 0;
    unsigned int len = 0;
    size = min(size, ring_buf->in - ring_buf->out);
    /* first get the data from fifo->out until the end of the buffer */
    len = min(size, ring_buf->size - (ring_buf->out & (ring_buf->size - 1)));
    memcpy(buffer, ring_buf->buffer + (ring_buf->out & (ring_buf->size - 1)), len);
    /* then get the rest (if any) from the beginning of the buffer */
    memcpy(buffer + len, ring_buf->buffer, size - len);
    ring_buf->out += size;
    return size;
}

//向缓冲区中存放数据
unsigned int play_ring_buffer_put(struct play_ring_buffer *ring_buf, uint8_t *buffer, unsigned int size)
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

unsigned int play_ring_buffer_len(const struct play_ring_buffer *ring_buf)
{
    unsigned int len = 0;
    len = __play_ring_buffer_len(ring_buf);
    return len;
}

unsigned int play_ring_buffer_avail(const struct play_ring_buffer *ring_buf)
{
    int availabe = 0;
    availabe = ring_buf->size - play_ring_buffer_len(ring_buf);
    return availabe;
}

unsigned int play_ring_buffer_get(struct play_ring_buffer *ring_buf, unsigned char *buffer, unsigned int size)
{
    unsigned int ret;
    ret = __play_ring_buffer_get(ring_buf, buffer, size);
    // buffer中没有数据
    if (ring_buf->in == ring_buf->out)
        ring_buf->in = ring_buf->out = 0;
    return ret;
}


unsigned int play_ring_buffer_put_block(struct play_ring_buffer *ring_buf, uint8_t *buffer, unsigned int size, int wait_ms)
{
    if (!ring_buf || !buffer)
        return 0;

    int write_len = 0;
    int ret = 0;
    while(write_len < size)
    {
        ret = play_ring_buffer_put(ring_buf, buffer+write_len, size-write_len);
        if(ret > 0)
        {
#if RING_BUFFER_BLOCK_ENABLE
            xEventGroupSetBits(ring_buf->ringbuff_sta,RINGBUFF_CAN_GET);
#endif
        }
        write_len += ret;
        if(write_len < size)
        {
#if RING_BUFFER_BLOCK_ENABLE
            // to do：理论上应该等待portMAX_DELAY，但是为了阻塞在这里而不自知，暂定等待200ms.
            EventBits_t uxBits = xEventGroupWaitBits(ring_buf->ringbuff_sta, RINGBUFF_CAN_PUT, pdTRUE, pdFALSE, pdMS_TO_TICKS(wait_ms));
            if( ( wait_ms > 0) && ((uxBits & RINGBUFF_CAN_PUT) == 0))
            {
                CLOGW("%s: wait for RINGBUFF_CAN_PUT bit time out!", __FUNCTION__);
                return write_len;
            }
#endif
        }

    }
    return size;
}

unsigned int play_ring_buffer_get_block(struct play_ring_buffer *ring_buf, uint8_t *buffer, unsigned int size, int wait_ms)
{
    if (!ring_buf || !buffer)
        return 0;

    int read_len = 0;
    int ret = 0;

    while(read_len < size)
    {
        ret = play_ring_buffer_get(ring_buf, buffer+read_len, size-read_len);
        if(ret > 0)
        {
#if RING_BUFFER_BLOCK_ENABLE
            xEventGroupSetBits(ring_buf->ringbuff_sta,RINGBUFF_CAN_PUT);
#endif
        }
        read_len += ret;
        if(read_len < size)
        {
#if RING_BUFFER_BLOCK_ENABLE
            EventBits_t uxBits = xEventGroupWaitBits(ring_buf->ringbuff_sta, RINGBUFF_CAN_GET, pdTRUE, pdFALSE, pdMS_TO_TICKS(wait_ms));
            if( ( wait_ms > 0) && ( (uxBits & RINGBUFF_CAN_GET) == 0) )
            {
                CLOGW("%s: wait for RINGBUFF_CAN_GET bit time out!", __FUNCTION__);
                return read_len;
            }
#endif
        }
    }

    return size;
}