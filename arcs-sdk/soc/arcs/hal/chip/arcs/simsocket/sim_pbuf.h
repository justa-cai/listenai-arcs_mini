#ifndef _SIM_PBUF_H_
#define _SIM_PBUF_H_

struct pbuf* pbuf_alloc(pbuf_layer layer, uint16_t length, pbuf_type type);
uint8_t pbuf_free(struct pbuf *p);
uint8_t sim_pbuf_header(struct pbuf *p, int16_t header_size_increment);
void sim_pbuf_cat(struct pbuf *h, struct pbuf *t);
struct pbuf *sim_pbuf_alloced_custom(pbuf_layer l, u16_t length, pbuf_type type, struct pbuf_custom *p, void *payload_mem, u16_t payload_mem_len);

#endif
