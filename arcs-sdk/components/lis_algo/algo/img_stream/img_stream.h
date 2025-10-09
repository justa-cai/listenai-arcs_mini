#ifndef __IMG_STREAM_H__
#define __IMG_STREAM_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMG_STREAM_START        0x00
#define IMG_STREAM_END          0xFFFFFFFF

enum img_type {
    IMG_RAW,
    IMG_STITCH,
    IMG_CUTLINE,
};

struct img_stream_msg {
    uint8_t type;
    uint32_t addr;
    uint16_t wdith;
    uint16_t height;
}__attribute__((packed));

typedef struct {
} img_stream_cbs_t;

typedef void (*on_data_received)(struct img_stream_msg *msg);

typedef struct {
    img_stream_cbs_t cbs;
    on_data_received observers[5];
} img_stream_t;

img_stream_t *img_stream_init(void);
void img_stream_register_on_recv(on_data_received cbs_recv);
void img_stream_unregister_on_recv(on_data_received cbs_recv);
void img_stream_unregister_all(void);

#ifdef __cplusplus
}
#endif

#endif
