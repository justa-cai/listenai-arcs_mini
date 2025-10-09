# pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <assert.h>

#include "log_print.h"
#include "Driver_CRYPTO.h"
#include "crypto.h"
#include "cache.h"

#define CSK_CRYPTO_OK                       0
#define CSK_CRYPTO_ERR_HARDWARE_ACQUIRED    -1
#define CSK_CRYPTO_ERR_HARDWARE_RELEASED    -2

#define CSK_CRYPTO_CHECK_RET(cond, ret)                         \
    do {                                                        \
        if (!(cond)) {                                          \
            CLOG("%s:%d, ret: %d", __FILE__, __LINE__, ret);    \
            return ret;                                         \
        }                                                       \
    }while(0)

#define CSK_CRYPTO_CHK_EXIT(f)                                      \
    do                                                              \
    {                                                               \
        if ((ret = (f)) != 0) {                                     \
            CLOG("%s %d, ret = %d", __FUNCTION__, __LINE__, ret);   \
            goto cleanup;                                           \
        }                                                           \
    } while (0)

#define CSK_CRYPTO_ASSERT(cond)  assert(cond)

typedef enum {
    CSK_CRYPTO_TYPE_AES,
    CSK_CRYPTO_TYPE_SHA,
    CSK_CRYPTO_TYPE_RSA,
    CSK_CRYPTO_TYPE_ECC,
} csk_crypto_type_e;

typedef enum {
    HASH_MODE_NONE,
    HASH_MODE_SHA1,
    HASH_MODE_SHA224,
    HASH_MODE_SHA256,
    HASH_MODE_SHA384,
    HASH_MODE_SHA512,
} hash_mode_e;

typedef void *crypto_handler; 

extern crypto_handler csk_crypto_acquire_hardware( csk_crypto_type_e type );
extern int csk_crypto_release_hardware( csk_crypto_type_e type );

extern void csk_dump_buf(char *info, uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif