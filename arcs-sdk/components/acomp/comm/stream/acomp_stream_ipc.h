#pragma once

#include "comm/stream/acomp_stream.h"

#ifdef __cplusplus
    extern "C" {
#endif


typedef struct{

    const char *cname;
    acomp_stream_direction_e direction;
    uint32_t index;
    uint32_t buffer_size;
    uint32_t num_descs;
    uint32_t kick_policy;                /* Kick policy: 0:manual/ >0: buffer count to kick */
    

}acomp_stream_chn_create_desc_t;

acomp_stream_channel_t* acomp_stream_ipc_channel_create(acomp_stream_t* stream,uint32_t chn,uint32_t dev_index,acomp_stream_chn_create_desc_t *desc);
int acomp_stream_ipc_channel_destroy(acomp_stream_t* stream, uint32_t dev_index, uint32_t chn);

#ifdef __cplusplus
    }
#endif
