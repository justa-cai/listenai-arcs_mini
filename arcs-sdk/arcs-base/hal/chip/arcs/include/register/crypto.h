/*
 * crypto.h
 *
 *  Created on: 2023/5/23
 *      Author: LAPTOP-07
 */

#ifndef MODULES_DRIVER_PRIVATE_INCLUDE_CRYPTO_H_
#define MODULES_DRIVER_PRIVATE_INCLUDE_CRYPTO_H_

#include "crypto_aes_reg.h"
#include "crypto_hsu_reg.h"
#include "crypto_ecc_reg.h"

#include "Driver_CRYPTO.h"

#include "arcs_ap.h"

// aes block size 128bit
#define CRYPTO_AES_BLOCK_SIZE   (16)

#define CHECK_RESOURCES(res)  do{\
    if(res != &crypto0_resources){\
        return CSK_DRIVER_ERROR_PARAMETER;\
    }\
}while(0)

#define DEBUG_LOG    0 //1 //
#if DEBUG_LOG
#include "log_print.h"

#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG

// CRYPTO flags
enum {
    CRYPTO_FLAG_INITIALIZED       = (1U << 0),
};

enum {
    CRYPTO_AES_KEY_SIZE_128            = 1,
    CRYPTO_AES_KEY_SIZE_192            = 2,
    CRYPTO_AES_KEY_SIZE_256            = 3,
};

enum {
    CRYPTO_AES_KEY_USER           = (2U),
    CRYPTO_AES_KEY_EFUSE1         = (4U),
    CRYPTO_AES_KEY_EFUSE2         = (5U),
    CRYPTO_AES_KEY_EFUSE3         = (6U),
};

enum
{
    CRYPTO_RSA_MODE_1024   = 1,
    CRYPTO_RSA_MODE_2048   = 2,
    CRYPTO_RSA_MODE_4096   = 3,
};

// hsu macro
/// Operation modes supported by HSU
enum hsu_modes
{
    HSU_MODE_TKIP_MIC = 0,
    HSU_MODE_AES_128_CMAC = 1,
    HSU_MODE_IP_CHK = 2,
    HSU_MODE_SHA_1 = 3,
    HSU_MODE_SHA_256 = 4,
    HSU_MODE_SHA_224 = 5,
    HSU_MODE_HMAC_SHA1 = 6,
    HSU_MODE_HMAC_SHA256 = 7,
    HSU_MODE_HMAC_SHA224 = 8,
    HSU_MODE_SHA_512 = 9,
    HSU_MODE_SHA_384 = 10,
    HSU_MODE_HMAC_SHA512 = 11,
    HSU_MODE_HMAC_SHA384 = 12,
    HSU_MODE_RSA_1024 = 13,
    HSU_MODE_RSA_2048 = 14,
    HSU_MODE_RSA_4096 = 15,
    HSU_MODE_RSA_256 = 16,
    HSU_MODE_RSA_512 = 17,
    HSU_MODE_RSA_768 = 18,
    HSU_MODE_AES = 19,
};

/// True if HSU support feature @b m, false otherwise
#define CRYPTO_HSU_SUPPORT(m) (crypto->hsu_reg->REG_REVISION.bit.m)
/// Wait until HSU set the done status
#define CRYPTO_HSU_WAIT_DONE(m)  while (!(crypto->hsu_reg->REG_STATUS_SET.bit.DONE_SET_##m));


typedef struct _AES_INFO {
    uint8_t key_size;
    uint8_t mode;
    uint8_t iv_len;
    uint8_t aad_len;
    uint8_t aad_flag;
    uint32_t done_len;   // enc/dec data length
    uint32_t last_len;   // data not process in current segment
    const uint32_t *source;    // source address
    uint32_t *result;    // result address
} AES_INFO;


typedef struct _SHA_INFO {
    uint8_t mode;
} SHA_INFO;


typedef struct _ECC_INFO {
    CRYPTO_ECC_CURVE *curve;
    uint32_t entry;
    uint32_t param_len;
} ECC_INFO;

typedef struct _RSA_INFO {
    uint8_t   mode;
    uint8_t   operation;
    uint8_t   padding_mode;
    uint8_t   msb_n; // for pss padding
    char     *oaep_label;
} RSA_INFO;

// CRYPTO Information (Run-Time)
typedef struct _CRYPTO_INFO {
    // Event callback
    CSK_CRYPTO_SignalEvent_t cb_event;
    void* workspace;

    uint8_t            flags;         // CRYPTO driver flags

    uint8_t            little_endian;

    uint8_t            power_on;

} CRYPTO_INFO;

// CRYPTO Resources definitions
typedef struct {
    CRYPTO_AES_RegDef        *aes_reg;           // Pointer to crypto peripheral
    CRYPTO_ECC_RegDef        *ecc_reg;
    CRYPTO_HSU_RegDef        *hsu_reg;
    uint32_t                irq_num_aes;       // IRQ Number
    uint32_t                irq_num_ecc;       // IRQ Number
    uint32_t                irq_num_hsu;       // IRQ Number
    void (*irq_handler_aes)(void);
    void (*irq_handler_ecc)(void);
    void (*irq_handler_hsu)(void);

    AES_INFO*          aes_info;
    SHA_INFO*          sha_info;
    ECC_INFO*          ecc_info;
    RSA_INFO*          rsa_info;
    CRYPTO_INFO*       info;
} const CRYPTO_RESOURCES;

extern const CRYPTO_RESOURCES crypto0_resources;

void crypto_aes_irq_handler(CRYPTO_RESOURCES *crypto);
void crypto_ecc_irq_handler(CRYPTO_RESOURCES *crypto);
void crypto_rsa_irq_handler(CRYPTO_RESOURCES *crypto);
void crypto_sha_irq_handler(CRYPTO_RESOURCES *crypto);

int32_t crypto_aes_set_mode(CRYPTO_RESOURCES *crypto, uint32_t mode);
int32_t crypto_aes_set_key_size(CRYPTO_RESOURCES *crypto, uint32_t key_size);
int32_t crypto_aes_set_key(CRYPTO_RESOURCES *crypto, uint32_t key_mode, uint32_t arg0);
int32_t crypto_aes_set_lengths(CRYPTO_RESOURCES *crypto, uint32_t* lens);
int32_t crypto_aes_set_iv(CRYPTO_RESOURCES *crypto, uint32_t* iv);
int32_t crypto_aes_get_mac(CRYPTO_RESOURCES *crypto, uint32_t* mac);

int32_t crypto_sha_reset(CRYPTO_RESOURCES *crypto);
int32_t crypto_sha_set_mode(CRYPTO_RESOURCES *crypto, uint32_t mode);
int32_t crypto_hash (CRYPTO_RESOURCES *crypto, uint32_t * p_source,
            uint32_t num_bytes, uint32_t * p_dest, uint32_t update);

int32_t crypto_rsa_set_mode(CRYPTO_RESOURCES *crypto, uint32_t mode);
int32_t crypto_rsa_set_padding_mode(CRYPTO_RESOURCES *crypto, uint32_t mode);
int32_t crypto_rsa_set_padding_label(CRYPTO_RESOURCES *crypto, char *label);

int32_t crypto_ecc_select_curve(CRYPTO_RESOURCES *crypto, CRYPTO_ECC_CURVE *p_curve);

int32_t crypto_hsu_set_key(CRYPTO_RESOURCES *crypto, uint32_t* key);

int32_t crypto_mod_exp(CRYPTO_RESOURCES *crypto, uint32_t *res, int *res_len, const uint32_t *val,
                       int val_len, const uint32_t *exponent, int exponent_len, const uint32_t *modulus, int modulus_len);

#endif // MODULES_DRIVER_PRIVATE_INCLUDE_CRYPTO_H_

