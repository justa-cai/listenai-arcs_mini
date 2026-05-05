#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>

typedef void (*sha256_done_cb_t)(void *ctx, const uint8_t *data, uint32_t len, void *user_data);

void *sha256_init(sha256_done_cb_t callback, void *user_data);
int sha256_start(void *ctx);
int sha256_update(void *ctx, const void *data, uint32_t len);
int sha256_final(void *ctx, uint8_t out[32]);
void sha256_free(void *ctx);

#endif
