#ifndef __MRPC_LWIP_API_CLIENT_H__
#define __MRPC_LWIP_API_CLIENT_H__

ls_err_t lwip_pbuf_alloc(const struct pbuf ** pbuf, pbuf_layer layer, u16_t length, pbuf_type type);

ls_err_t lwip_pbuf_free(struct pbuf * p, uint8_t * count);


#endif