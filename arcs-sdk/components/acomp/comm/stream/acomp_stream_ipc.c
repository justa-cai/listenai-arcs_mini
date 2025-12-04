#include <stdio.h>
#include <string.h>

#include "acomp_stream_ipc.h"
#include "utils/acomp_err.h"

#define TAG "stream_ipc"
#include "lisa_log.h"

acomp_stream_channel_t* acomp_stream_ipc_channel_create(acomp_stream_t* stream,uint32_t chn,uint32_t dev_index,acomp_stream_chn_create_desc_t *desc){

    uint32_t mem_size;
    uint8_t* mem_ptr;
    int ret;
    acomp_ipc_stream_create_desc_t *ipc_desc;

    acomp_stream_channel_desc_t chn_desc = {
        .cname = desc->cname,
        .direction = desc->direction,
        .vq_id = desc->index,
        .ring = {
            .phy_addr = NULL,
            .align = 32,
            .num_descs = desc->num_descs,
            .pad = 32,
        },
        .buffer_size = desc->buffer_size,
        .callback_fc = NULL,
        .notify_fc = NULL,
        .kick_policy = desc->kick_policy,
        .user_priv = NULL,
    };

    mem_size = acomp_stream_calc_mem_size (chn_desc.ring.num_descs, chn_desc.buffer_size, chn_desc.ring.align);
    if (mem_size == 0) {
        LISA_LOGE(TAG, "Failed to calculate stream memory size");
        return NULL;
    }

    mem_ptr = psram_malloc_align(chn_desc.ring.align, mem_size);
    if (mem_ptr == NULL) {
        LISA_LOGE(TAG, "Failed to allocate stream memory");
        return NULL;
    }
    memset(mem_ptr, 0, mem_size);

    chn_desc.ring.phy_addr = mem_ptr;
    acomp_stream_channel_t *channel = stream->ops.channel_create(stream, &chn_desc);

    if(!channel){
        LISA_LOGE(TAG, "Failed to create channel %d", chn);
        return NULL;
    }

    ipc_desc = psram_malloc_align(chn_desc.ring.align, sizeof(acomp_ipc_stream_create_desc_t));
    if (ipc_desc == NULL) {
        LISA_LOGE(TAG, "Failed to allocate ipc stream create memory");
        stream->ops.channel_destroy(channel);
        return NULL;
    }

    snprintf(ipc_desc->name, sizeof(ipc_desc->name), "%s", chn_desc.cname);
    ipc_desc->direction = chn_desc.direction == ACOMP_STREAM_DIRECTION_M2R ? 0 : 1;
    ipc_desc->index = chn_desc.vq_id;
    ipc_desc->phy_addr = mem_ptr;
    ipc_desc->mem_size = mem_size;
    ipc_desc->align = chn_desc.ring.align;
    ipc_desc->buffer_size = chn_desc.buffer_size;
    ipc_desc->kick_policy = chn_desc.kick_policy;
    
    ret = acomp_ipc_build_frame_send_sync(dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                        ACOMP_IPC_CMD_STREAM_CREATE, 0, ipc_desc, sizeof(acomp_ipc_stream_create_desc_t));
    if(ret != ACOMP_ERR_OK){
        stream->ops.channel_destroy(channel);
        return NULL;
    }
    psram_free(ipc_desc);
    return channel;
}


int acomp_stream_ipc_channel_destroy(uint32_t chn){
    
}
