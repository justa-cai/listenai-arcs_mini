#ifndef __ACOMP_STREAM_H__
#define __ACOMP_STREAM_H__

#include <stdint.h>
#include <stdbool.h>
#include "../virtio/virtqueue.h"

/*
 * IMPORTANT: Compile-time role definition
 * 
 * Define ACOMP_STREAM_ROLE_MASTER to indicate current program runs on Master core
 * - If defined: Master role (device side for M2R, driver side for R2M)
 * - If NOT defined: Remote role (driver side for M2R, device side for R2M)
 * 
 * This macro determines automatic buffer pre-filling behavior:
 * - Driver side (RX): Pre-fills available ring with empty buffers
 * - Device side (TX): Gets buffers from available ring to write data
 * 
 * Example usage in build system:
 *   Master build: -DACOMP_STREAM_ROLE_MASTER
 *   Remote build: (no flag needed, defaults to Remote)
 */

#if defined(CONFIG_ACOMP_REMOTE)
#define ACOMP_STREAM_ROLE_REMOTE
#else
#define ACOMP_STREAM_ROLE_MASTER  
#endif

#if defined (ACOMP_STREAM_ROLE_MASTER)
#include "../ipc/acomp_ipc.h"
#endif

#define ACOMP_STREAM_NAME_MAX_LEN    (16)
#define ACOMP_STREAM_MAX_CHANNEL   (4)

/* Kick policy definitions */
#define ACOMP_STREAM_KICK_IMMEDIATE  (0)  /* Kick immediately after each buffer */
#define ACOMP_STREAM_KICK_BATCH      (1)  /* Kick after multiple buffers */
#define ACOMP_STREAM_KICK_MANUAL     (2)  /* Manual kick by upper layer */
struct acomp_stream;
typedef enum{
    ACOMP_STREAM_DIRECTION_M2R, /* master to remote */
    ACOMP_STREAM_DIRECTION_R2M, /* remote to master */
}acomp_stream_direction_e;

typedef struct{
    char name[ACOMP_STREAM_NAME_MAX_LEN];
    acomp_stream_direction_e direction;
    uint32_t idx;
    struct virtqueue* vq;               /* Associated virtqueue */
    uint32_t kick_policy;                /* Kick policy */
    uint32_t kick_count;                 /* Kick count */
    void* user_priv;                    /* User private data */
    void* priv;
}acomp_stream_channel_t;


typedef struct{
    const char* cname;                    /* Channel name for debugging */
    acomp_stream_direction_e direction; /* Channel direction */
    uint16_t vq_id;                     /* VirtQueue ID */
    struct vring_alloc_info ring;       /* VirtQueue ring memory info */
    uint32_t buffer_size;               /* Size of each buffer (required for R2M channels) */
    vq_callback *callback_fc;            /* Optional: callback when data available */
    vq_notify *notify_fc;                /* Optional: notify function to peer */
    uint32_t kick_policy;                /* Kick policy: 0:manual/ >0: buffer count to kick */
    void* user_priv;                    /* User private data */
}acomp_stream_channel_desc_t;

typedef struct{
    /* Channel management */
    acomp_stream_channel_t *(*channel_create)(struct acomp_stream* stream, acomp_stream_channel_desc_t *desc);
    int (*channel_destroy)(acomp_stream_channel_t* channel);
    
    /* Zero-copy buffer operations (symmetric for TX and RX) */
    void *(*tx_buffer_alloc)(acomp_stream_channel_t* channel, uint32_t* len, uint16_t* desc_idx);
    int (*tx_buffer_submit)(acomp_stream_channel_t* channel, void *buffer, uint32_t len, uint16_t desc_idx);
    void *(*rx_buffer_get)(acomp_stream_channel_t* channel, uint32_t* len, uint16_t* desc_idx);
    int (*rx_buffer_release)(acomp_stream_channel_t* channel, void *buffer, uint32_t len, uint16_t desc_idx);

    /* Control operations */
    int (*kick)(acomp_stream_channel_t* channel);
    uint32_t (*get_buffer_len)(acomp_stream_channel_t* channel, uint16_t desc_idx);
    
}acomp_stream_ops_t;

typedef struct acomp_stream{
    acomp_stream_channel_t *ch[ACOMP_STREAM_MAX_CHANNEL];
    acomp_stream_ops_t ops;
    uint32_t channel_count;             /* Number of active channels */
    uint32_t  dev_index;
}acomp_stream_t;


/* Error codes */
#define ACOMP_STREAM_SUCCESS             (0)
#define ACOMP_STREAM_ERROR_INVALID_PARAM (-1)
#define ACOMP_STREAM_ERROR_NO_MEMORY     (-2)
#define ACOMP_STREAM_ERROR_CHANNEL_FULL  (-3)
#define ACOMP_STREAM_ERROR_WRONG_DIRECTION (-4)
#define ACOMP_STREAM_ERROR_NO_BUFFER     (-5)
#define ACOMP_STREAM_ERROR_CREATE_FAILED  (-6)

/* Function declarations */
acomp_stream_t* acomp_stream_create(uint32_t dev_index);
int acomp_stream_destroy(acomp_stream_t* stream);
uint16_t acomp_stream_calc_buffer_num(uint32_t mem_size, uint32_t buffer_size, uint32_t align);
uint32_t acomp_stream_calc_mem_size(uint32_t num_descs, uint32_t buffer_size, uint32_t align);


#endif