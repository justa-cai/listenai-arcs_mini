#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include "acomp_stream.h"
#include "../acomp.h"
#include "../virtio/virtqueue.h"
#include "../virtio/virtio_ring.h"

#include "sysheap.h"

#define TAG "acomp_stream"
#include "lisa_log.h"

/* Private function declarations */
static acomp_stream_channel_t *acomp_stream_channel_create(acomp_stream_t *stream, acomp_stream_channel_desc_t *desc);
static int acomp_stream_channel_destroy(acomp_stream_channel_t *channel);

static void *(acomp_stream_tx_buffer_alloc)(acomp_stream_channel_t *channel, uint32_t *len, uint16_t *desc_idx);
static int(acomp_stream_tx_buffer_submit)(acomp_stream_channel_t *channel, void *buffer, uint32_t len,
                                          uint16_t desc_idx);
static void *(acomp_stream_rx_buffer_get)(acomp_stream_channel_t *channel, uint32_t *len, uint16_t *desc_idx);
static int(acomp_stream_rx_buffer_release)(acomp_stream_channel_t *channel, void *buffer, uint32_t len,
                                           uint16_t desc_idx);

static int acomp_stream_kick(acomp_stream_channel_t *channel);
static int acomp_stream_enable_cb(acomp_stream_channel_t *channel);
static void acomp_stream_disable_cb(acomp_stream_channel_t *channel);
static uint32_t acomp_stream_get_buffer_len(acomp_stream_channel_t *channel, uint16_t desc_idx);

/**
 * Calculate number of buffers that can fit in given memory
 * @param mem_size: Total shared memory size in bytes
 * @param buffer_size: Size of each buffer in bytes
 * @param align: Alignment requirement (typically 32 or 64)
 * @return: Number of buffers (must be power of 2), 0 if memory too small
 *
 * Memory layout: [vring metadata][buffer0][buffer1]...[bufferN-1]
 * vring metadata includes: descriptor table, available ring, used ring
 * Each buffer is aligned to 'align' boundary
 */
uint16_t acomp_stream_calc_buffer_num(uint32_t mem_size, uint32_t buffer_size, uint32_t align)
{
    if (mem_size < 128 || buffer_size == 0 || align == 0) {
        return 0;
    }

    /* Align buffer_size up to alignment boundary */
    uint32_t aligned_buffer_size = (buffer_size + align - 1) & ~(align - 1);

    /* Try powers of 2 from largest to smallest */
    uint16_t num = 1;

    /* Find the largest power of 2 that might fit */
    while (num < 8192 && (num * aligned_buffer_size) < mem_size) {
        num <<= 1;
    }

    /* Try smaller values until we find one that fits */
    while (num >= 1) {
        int32_t vring_metadata_size = vring_size(num, align);
        uint32_t total_buffer_size = num * aligned_buffer_size;
        uint32_t total_required = vring_metadata_size + total_buffer_size;

        if (total_required <= mem_size) {
            CLOGD("[%s] mem_size=%u, buffer_size=%u->%u (aligned): num_buffers=%u (vring=%d, buffers=%u, total=%u)\n",
                  __FUNCTION__, mem_size, buffer_size, aligned_buffer_size, num, vring_metadata_size, total_buffer_size,
                  total_required);
            return num;
        }

        num >>= 1; /* Try half the size */
    }
    
    return 0;
}

/**
 * Calculate required memory size for given number of buffers
 * @param num_descs: Number of descriptors (must be power of 2)
 * @param buffer_size: Size of each buffer in bytes
 * @param align: Alignment requirement (typically 32 or 64)
 * @return: Total memory size required in bytes, 0 on error
 *
 * This is the inverse of acomp_stream_calc_buffer_num()
 * Memory layout: [vring metadata][buffer0][buffer1]...[bufferN-1]
 * Each buffer is aligned to 'align' boundary
 */
uint32_t acomp_stream_calc_mem_size(uint32_t num_descs, uint32_t buffer_size, uint32_t align)
{
    if (num_descs == 0 || buffer_size == 0 || align == 0) {
        CLOGE("[%s] Invalid parameters: num_descs=%u, buffer_size=%u, align=%u\n", __FUNCTION__, num_descs, buffer_size,
              align);
        return 0;
    }

    /* Validate num_descs is power of 2 */
    if ((num_descs & (num_descs - 1)) != 0) {
        CLOGE("[%s] num_descs=%u must be power of 2\n", __FUNCTION__, num_descs);
        return 0;
    }

    /* Calculate vring metadata size */
    int32_t vring_metadata_size = vring_size(num_descs, align);
    if (vring_metadata_size < 0) {
        CLOGE("[%s] Invalid vring_size result: %d\n", __FUNCTION__, vring_metadata_size);
        return 0;
    }

    /* Align buffer_size up to alignment boundary to ensure each buffer is aligned */
    uint32_t aligned_buffer_size = (buffer_size + align - 1) & ~(align - 1);

    /* Calculate total required memory */
    uint32_t total_buffer_size = num_descs * aligned_buffer_size;
    uint32_t total_required = vring_metadata_size + total_buffer_size;


    return total_required;
}

/**
 * Initialize acomp_stream component
 * @param stream: acomp_stream instance to initialize
 * @return: ACOMP_STREAM_SUCCESS on success, error code otherwise
 */

acomp_stream_t *acomp_stream_create(uint32_t dev_index)
{
    acomp_stream_t *stream = psram_malloc(sizeof(acomp_stream_t));
    if (!stream) {
        CLOGE("[%s] Invalid parameter: stream is NULL\n", __FUNCTION__);
        return NULL;
    }

    memset(stream, 0, sizeof(acomp_stream_t));

    /* Initialize operations */
    stream->ops.channel_create = acomp_stream_channel_create;
    stream->ops.channel_destroy = acomp_stream_channel_destroy;

    /* TX/RX buffer operations */
    stream->ops.tx_buffer_alloc = acomp_stream_tx_buffer_alloc;
    stream->ops.tx_buffer_submit = acomp_stream_tx_buffer_submit;
    stream->ops.rx_buffer_get = acomp_stream_rx_buffer_get;
    stream->ops.rx_buffer_release = acomp_stream_rx_buffer_release;

    /* Control operations */
    stream->ops.kick = acomp_stream_kick;
    stream->ops.get_buffer_len = acomp_stream_get_buffer_len;

    stream->channel_count = 0;
    stream->dev_index = dev_index; /* Store dev pointer in REMOTE mode */

    CLOGD("[%s] acomp_stream create successfully\n", __FUNCTION__);
    return stream;
}

/**
 * Deinitialize acomp_stream component
 * @param stream: acomp_stream instance to deinitialize
 * @return: ACOMP_STREAM_SUCCESS on success, error code otherwise
 */
int acomp_stream_destroy(acomp_stream_t *stream)
{
    if (!stream) {
        return ACOMP_STREAM_ERROR_INVALID_PARAM;
    }

    /* Destroy all active channels */
    for (uint32_t i = 0; i < ACOMP_STREAM_MAX_CHANNEL; i++) {
        if (stream->ch[i] != NULL && stream->ch[i]->vq != NULL) {
            acomp_stream_channel_destroy(stream->ch[i]);
            stream->ch[i] = NULL;
        }
    }
    psram_free(stream);
    CLOGD("[%s] acomp_stream destroy\n", __FUNCTION__);
    return ACOMP_STREAM_SUCCESS;
}

/**
 * Create a new channel
 * @param stream: Stream instance to create channel in
 * @param desc: Channel description
 * @return: Pointer to created channel, NULL on failure
 */
static acomp_stream_channel_t *acomp_stream_channel_create(acomp_stream_t *stream, acomp_stream_channel_desc_t *desc)
{
    acomp_stream_channel_t *channel;

    if (!stream || !desc || !desc->cname) {
        CLOGE("[%s] Invalid parameter: stream, desc or name is NULL\n", __FUNCTION__);
        return NULL;
    }

    /* Validate ring parameters */
    if (desc->ring.num_descs == 0 || (desc->ring.num_descs & (desc->ring.num_descs - 1)) != 0) {
        CLOGE("[%s] Invalid ring size: %d (must be power of 2)\n", __FUNCTION__, desc->ring.num_descs);
        return NULL;
    }

    if (!desc->ring.phy_addr) {
        CLOGE("[%s] Invalid ring physical address\n", __FUNCTION__);
        return NULL;
    }

    /* Validate buffer_size (recommended for zero-copy transfer) */
    if (desc->buffer_size == 0) {
        CLOGW("[%s] Warning: buffer_size not provided, zero-copy transfer won't be available\n", __FUNCTION__);
        /* Allow creation without pre-filling for backward compatibility */
    }

    channel = psram_malloc(sizeof(acomp_stream_channel_t));
    if (!channel) {
        CLOGE("[%s] No available channel slots\n", __FUNCTION__);
        return NULL;
    }

    /* Initialize channel */
    memset(channel, 0, sizeof(acomp_stream_channel_t));
    snprintf(channel->name, sizeof(channel->name), "%s", desc->cname);
    channel->direction = desc->direction;
    channel->idx = desc->vq_id;
    channel->kick_policy = desc->kick_policy;
    channel->user_priv = desc->user_priv;
    channel->priv = stream;

    /* Create virtqueue */
    int32_t ret =
        virtqueue_create(desc->vq_id, desc->cname, &desc->ring, desc->callback_fc, desc->notify_fc, &channel->vq);
    if (ret != VQUEUE_SUCCESS) {
        CLOGE("[%s] Failed to create virtqueue: %d\n", __FUNCTION__, ret);
        memset(channel, 0, sizeof(acomp_stream_channel_t));
        return NULL;
    }

    /* Set channel as virtqueue's private data for callback */
    channel->vq->priv = channel;



#ifdef ACOMP_STREAM_ROLE_MASTER
if (desc->direction == ACOMP_STREAM_DIRECTION_R2M)
#else
if (desc->direction == ACOMP_STREAM_DIRECTION_M2R)
#endif
    {
        /* Initialize virtqueue ring */
        vq_ring_init(channel->vq);
        if (desc->buffer_size > 0) {
            /* Calculate buffer pool start address:
            * buffer_pool = shared_mem_base + vring_metadata_size
            */
            int32_t vring_metadata_size = vring_size(desc->ring.num_descs, desc->ring.align);
            uint8_t *buffer_pool = (uint8_t *)desc->ring.phy_addr + vring_metadata_size;

            /* Align buffer_size to ensure each buffer is properly aligned */
            uint32_t aligned_buffer_size = (desc->buffer_size + desc->ring.align - 1) & ~(desc->ring.align - 1);

            /* Fill all descriptors with available buffers */
            for (uint16_t i = 0; i < desc->ring.num_descs; i++) {
                void *buffer = buffer_pool + (i * aligned_buffer_size);
                ret = virtqueue_fill_avail_buffers(channel->vq, buffer, desc->buffer_size);
                

                if (ret != VQUEUE_SUCCESS) {
                    CLOGE("[%s] Failed to pre-fill virtqueue buffer %d: %d\n", __FUNCTION__, i, ret);
                    virtqueue_free(channel->vq);
                    psram_free(channel);
                    return NULL;
                }
            }
        }
    }

    stream->channel_count++;
    CLOGI("[%s]Acomp stream channel %p,%p,'%s' created successfully ,vring phy addr=%p, num_descs=%d, buffer_size=%u, "
          "direction=%s, role=%s\n",
          __FUNCTION__, channel, channel->vq, desc->cname, desc->ring.phy_addr, desc->ring.num_descs, desc->buffer_size,
          desc->direction == ACOMP_STREAM_DIRECTION_M2R ? "M2R" : "R2M",
#ifdef ACOMP_STREAM_ROLE_MASTER
          "Master"
#else
          "Remote"
#endif
    );

    return channel;
}

/**
 * Destroy a channel
 * @param channel: Channel to destroy
 * @return: ACOMP_STREAM_SUCCESS on success, error code otherwise
 */
static int acomp_stream_channel_destroy(acomp_stream_channel_t *channel)
{
    if (!channel) {
        CLOGE("[%s] Invalid parameter: channel is NULL\n", __FUNCTION__);
        return ACOMP_STREAM_ERROR_INVALID_PARAM;
    }

    CLOGD("[%s] Destroying channel '%s' (idx=%d)\n", __FUNCTION__, channel->name, channel->idx);

    /* Get parent stream for updating channel count (before memset) */
    acomp_stream_t *stream = (acomp_stream_t *)channel->priv;

    /* Disable virtqueue callback if exists */
    if (channel->vq) {
        /* Free virtqueue */
        virtqueue_free(channel->vq);
        channel->vq = NULL;
    }

    /* Update parent stream channel count */
    if (stream && stream->channel_count > 0) {
        stream->channel_count--;
    }

    /* Free channel memory */
    psram_free(channel);

    CLOGD("[%s] Channel destroyed successfully\n", __FUNCTION__);
    return ACOMP_STREAM_SUCCESS;
}

static void *(acomp_stream_tx_buffer_alloc)(acomp_stream_channel_t *channel, uint32_t *len, uint16_t *desc_idx)
{
    void *p_buf = NULL;

    if (!channel || !channel->vq) {
        CLOGE("[%s] Invalid parameters\n", __FUNCTION__);
        return NULL;
    }

#if defined(ACOMP_STREAM_ROLE_MASTER)
    if (channel->direction == ACOMP_STREAM_DIRECTION_M2R)
#else
    if (channel->direction == ACOMP_STREAM_DIRECTION_R2M)
#endif
    {
        p_buf = virtqueue_get_available_buffer(channel->vq, desc_idx, len);
    }


    /* invalidate cache before use buffer */
    if (p_buf != NULL) {
        env_cache_invalidate(p_buf, *len);
    }

    return p_buf;
}
static int(acomp_stream_tx_buffer_submit)(acomp_stream_channel_t *channel, void *buffer, uint32_t len,
                                          uint16_t desc_idx)
{
    int ret = ACOMP_STREAM_ERROR_WRONG_DIRECTION;
    if (!channel || !channel->vq || !buffer || !len) {
        return ACOMP_STREAM_ERROR_INVALID_PARAM;
    }

#if defined(ACOMP_STREAM_ROLE_MASTER)
    if (channel->direction == ACOMP_STREAM_DIRECTION_M2R)
#else
    if (channel->direction == ACOMP_STREAM_DIRECTION_R2M)
#endif
    {
        /* flush cache before submit buffer */
        env_cache_flush(buffer, len);
        ret = virtqueue_add_consumed_buffer(channel->vq, desc_idx, len);
        /* Kick based on policy */
        channel->kick_count++;
        if ((channel->kick_policy != 0) && (channel->kick_count >= channel->kick_policy)) {
            virtqueue_kick(channel->vq);
            channel->kick_count = 0;
        }
        
    }

    return ret;
}

static void *(acomp_stream_rx_buffer_get)(acomp_stream_channel_t *channel, uint32_t *len, uint16_t *desc_idx)
{
    void *p_buf = NULL;

    if (!channel || !channel->vq) {
        CLOGE("[%s] Invalid parameters\n", __FUNCTION__);
        return NULL;
    }

#if defined(ACOMP_STREAM_ROLE_MASTER)
    if (channel->direction == ACOMP_STREAM_DIRECTION_R2M) 
#else
    if (channel->direction == ACOMP_STREAM_DIRECTION_M2R) 
#endif
    {
        p_buf = virtqueue_get_buffer(channel->vq, len, desc_idx);
    }

    /* invalidate cache before use buffer */
    if (p_buf != NULL) {
        env_cache_invalidate(p_buf, *len);
    }

    return p_buf;
}

static int(acomp_stream_rx_buffer_release)(acomp_stream_channel_t *channel, void *buffer, uint32_t len,
                                           uint16_t desc_idx)
{

    int ret = 0;

    if (!channel || !channel->vq || !buffer) {
        return ACOMP_STREAM_ERROR_INVALID_PARAM;
    }
    /* flush cache before submit buffer */
    env_cache_flush(buffer, len);

#if defined(ACOMP_STREAM_ROLE_MASTER)
    if (channel->direction == ACOMP_STREAM_DIRECTION_R2M) 
#else
    if (channel->direction == ACOMP_STREAM_DIRECTION_M2R) 
#endif
    {
        ret = virtqueue_add_buffer(channel->vq, desc_idx);
    }
    return ret;
}

/**
 * Manually kick the virtqueue to notify peer
 * @param channel: Channel to kick
 * @return: ACOMP_STREAM_SUCCESS on success, error code otherwise
 */
static int acomp_stream_kick(acomp_stream_channel_t *channel)
{
    if (!channel || !channel->vq) {
        return ACOMP_STREAM_ERROR_INVALID_PARAM;
    }

    virtqueue_kick(channel->vq);

    return ACOMP_STREAM_SUCCESS;
}

/**
 * Get buffer length by descriptor index
 * @param channel: Channel
 * @param desc_idx: Descriptor index
 * @return: Buffer length, 0 on error
 */
static uint32_t acomp_stream_get_buffer_len(acomp_stream_channel_t *channel, uint16_t desc_idx)
{
    if (!channel || !channel->vq) {
        return 0;
    }

    return virtqueue_get_buffer_length(channel->vq, desc_idx);
}
