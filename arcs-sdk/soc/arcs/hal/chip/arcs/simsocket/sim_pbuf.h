#ifndef _SIM_PBUF_H_
#define _SIM_PBUF_H_



struct pbuf* sim_pbuf_alloc(pbuf_layer layer, uint16_t length, pbuf_type type);
uint8_t sim_pbuf_free(struct pbuf *p);
uint8_t sim_pbuf_header(struct pbuf *p, int16_t header_size_increment);
void sim_pbuf_cat(struct pbuf *h, struct pbuf *t);
#endif
