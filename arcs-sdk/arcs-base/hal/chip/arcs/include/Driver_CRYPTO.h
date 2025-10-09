/*
 * Driver_CRYPTO.h
 *
 *  Created on: 2020/9/25
 *      Author: USER
 */

#ifndef INCLUDE_DRIVER_DRIVER_CRYPTO_H_
#define INCLUDE_DRIVER_DRIVER_CRYPTO_H_


#include "Driver_Common.h"

#define CSK_CRYPTO_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,02)  /* API version */

#define CRYPTO_MAX_PACKAGE_SIZE                 (0x3c00)

/// Maximum length in bits of ECC supported
#define CRYPTO_ECC_MAX_LEN                      (512)

/// Maximum length in bits of RSA supported
#define CRYPTO_RSA_MAX_LEN                      (4096)

/****** CRYPTO Control Codes *****/
/*----- CRYPTO Control Codes: AES Parameters: Key mode -----*/
#define CSK_CRYPTO_AES_KEY_MODE_Pos               0
#define CSK_CRYPTO_AES_KEY_MODE_Msk               (7UL << CSK_CRYPTO_AES_KEY_MODE_Pos)
#define CSK_CRYPTO_AES_KEY_MODE_ROOT              (1UL << CSK_CRYPTO_AES_KEY_MODE_Pos)
#define CSK_CRYPTO_AES_KEY_MODE_USER              (2UL << CSK_CRYPTO_AES_KEY_MODE_Pos)
#define CSK_CRYPTO_AES_KEY_MODE_TEST              (3UL << CSK_CRYPTO_AES_KEY_MODE_Pos)
#define CSK_CRYPTO_AES_KEY_MODE_EFUSE1            (4UL << CSK_CRYPTO_AES_KEY_MODE_Pos)
#define CSK_CRYPTO_AES_KEY_MODE_EFUSE2            (5UL << CSK_CRYPTO_AES_KEY_MODE_Pos)
#define CSK_CRYPTO_AES_KEY_MODE_EFUSE_USER        (6UL << CSK_CRYPTO_AES_KEY_MODE_Pos)

// set AES mode
#define CSK_CRYPTO_SET_AES_MODE_Pos               3
#define CSK_CRYPTO_SET_AES_MODE_Msk               (1UL << CSK_CRYPTO_SET_AES_MODE_Pos)
#define CSK_CRYPTO_SET_AES_MODE                   (1UL << CSK_CRYPTO_SET_AES_MODE_Pos)
// AES mode
#define CSK_CRYPTO_AES_MODE_ECB                   (1UL)
#define CSK_CRYPTO_AES_MODE_CBC                   (2UL)
#define CSK_CRYPTO_AES_MODE_CTR                   (3UL)
#define CSK_CRYPTO_AES_MODE_CCM                   (4UL)
#define CSK_CRYPTO_AES_MODE_CMAC                  (5UL)
#define CSK_CRYPTO_AES_MODE_GCM                   (6UL)

/*----- CRYPTO Control Codes: AES Parameters: Set AES Init Vector -----*/
#define CSK_CRYPTO_SET_AES_IV_Pos                 5
#define CSK_CRYPTO_SET_AES_IV_Msk                 (1UL << CSK_CRYPTO_SET_AES_IV_Pos)
#define CSK_CRYPTO_SET_AES_IV                     (1UL << CSK_CRYPTO_SET_AES_IV_Pos)

#define CSK_CRYPTO_RESET_HASH_Pos                 7
#define CSK_CRYPTO_RESET_HASH_Msk                 (1UL << CSK_CRYPTO_RESET_HASH_Pos)
#define CSK_CRYPTO_RESET_HASH                     (1UL << CSK_CRYPTO_RESET_HASH_Pos)

#define CSK_CRYPTO_SET_AES_KEY_SIZE_Pos           17
#define CSK_CRYPTO_SET_AES_KEY_SIZE_Msk           (3UL << CSK_CRYPTO_SET_AES_KEY_SIZE_Pos)
#define CSK_CRYPTO_SET_AES_KEY_SIZE_128           (1UL << CSK_CRYPTO_SET_AES_KEY_SIZE_Pos)
#define CSK_CRYPTO_SET_AES_KEY_SIZE_192           (2UL << CSK_CRYPTO_SET_AES_KEY_SIZE_Pos)
#define CSK_CRYPTO_SET_AES_KEY_SIZE_256           (3UL << CSK_CRYPTO_SET_AES_KEY_SIZE_Pos)

// set parameters length for aes cmac, ctr, ccm, gcm mode
// [MAC length, IV length, aad length, data length]
#define CSK_CRYPTO_SET_AES_LENGTHS_Pos            19
#define CSK_CRYPTO_SET_AES_LENGTHS_Msk            (1UL << CSK_CRYPTO_SET_AES_LENGTHS_Pos)
#define CSK_CRYPTO_SET_AES_LENGTHS                (1UL << CSK_CRYPTO_SET_AES_LENGTHS_Pos)

// CMAC/CCM/GCM MAC
#define CSK_CRYPTO_GET_AES_MAC_Pos                22
#define CSK_CRYPTO_GET_AES_MAC_Msk                (1UL << CSK_CRYPTO_GET_AES_MAC_Pos)
#define CSK_CRYPTO_GET_AES_MAC                    (1UL << CSK_CRYPTO_GET_AES_MAC_Pos)

// set hash mode
#define CSK_CRYPTO_SET_HASH_MODE_Pos              23
#define CSK_CRYPTO_SET_HASH_MODE_Msk              (1UL << CSK_CRYPTO_SET_HASH_MODE_Pos)
#define CSK_CRYPTO_SET_HASH_MODE                  (1UL << CSK_CRYPTO_SET_HASH_MODE_Pos)
// hash mode
#define CSK_CRYPTO_HASH_SHA1                      (1UL)
#define CSK_CRYPTO_HASH_SHA224                    (2UL)
#define CSK_CRYPTO_HASH_SHA256                    (3UL)
#define CSK_CRYPTO_HASH_SHA384                    (4UL)
#define CSK_CRYPTO_HASH_SHA512                    (5UL)

// only valid for rsa and ecc
#define CSK_CRYPTO_SET_LITTLE_ENDIAN_Pos          24
#define CSK_CRYPTO_SET_LITTLE_ENDIAN_Msk          (1UL << CSK_CRYPTO_SET_LITTLE_ENDIAN_Pos)
#define CSK_CRYPTO_SET_LITTLE_ENDIAN              (1UL << CSK_CRYPTO_SET_LITTLE_ENDIAN_Pos)

// set rsa mode
#define CSK_CRYPTO_SET_RSA_MODE_Pos               25
#define CSK_CRYPTO_SET_RSA_MODE_Msk               (3UL << CSK_CRYPTO_SET_RSA_MODE_Pos)
#define CSK_CRYPTO_SET_RSA_RSA1024                (1UL << CSK_CRYPTO_SET_RSA_MODE_Pos)
#define CSK_CRYPTO_SET_RSA_RSA2048                (2UL << CSK_CRYPTO_SET_RSA_MODE_Pos)
#define CSK_CRYPTO_SET_RSA_RSA4096                (3UL << CSK_CRYPTO_SET_RSA_MODE_Pos)

// set rsa padding mode
#define CSK_CRYPTO_SET_RSA_PADDING_MODE_Pos       27
#define CSK_CRYPTO_SET_RSA_PADDING_MODE_Msk       (1UL << CSK_CRYPTO_SET_RSA_PADDING_MODE_Pos)
#define CSK_CRYPTO_SET_RSA_PADDING_MODE           (1UL << CSK_CRYPTO_SET_RSA_PADDING_MODE_Pos)
// rsa padding mode
#define CSK_CRYPTO_RSA_PADDING_NONE               (0)
#define CSK_CRYPTO_RSA_PADDING_PKCS1              (1)
#define CSK_CRYPTO_RSA_PADDING_PSS                (2)
#define CSK_CRYPTO_RSA_PADDING_X931               (3)
#define CSK_CRYPTO_RSA_PADDING_OAEP               (4)
// rsa oaep label
#define CSK_CRYPTO_SET_RSA_PADDING_LABEL_Pos      28
#define CSK_CRYPTO_SET_RSA_PADDING_LABEL_Msk      (1UL << CSK_CRYPTO_SET_RSA_PADDING_LABEL_Pos)
#define CSK_CRYPTO_SET_RSA_PADDING_LABEL          (1UL << CSK_CRYPTO_SET_RSA_PADDING_LABEL_Pos)

// select ecc curve
#define CSK_CRYPTO_SET_ECC_CURVE_Pos              29
#define CSK_CRYPTO_SET_ECC_CURVE_Msk              (1UL << CSK_CRYPTO_SET_ECC_CURVE_Pos)
#define CSK_CRYPTO_SET_ECC_CURVE                  (1UL << CSK_CRYPTO_SET_ECC_CURVE_Pos)

// set Michael MIC Key
#define CSK_CRYPTO_SET_MIC_KEY_Pos                30
#define CSK_CRYPTO_SET_MIC_KEY_Msk                (1UL << CSK_CRYPTO_SET_MIC_KEY_Pos)
#define CSK_CRYPTO_SET_MIC_KEY                    (1UL << CSK_CRYPTO_SET_MIC_KEY_Pos)


// the IP Checksum HW module can use simultaneous with other module
typedef enum _CRYPTO_HW_
{
    CSK_CRYPTO_HW_IP_CHECKSUM = (1UL << 0),
    CSK_CRYPTO_HW_AES_SHA     = (1UL << 1),
    CSK_CRYPTO_HW_ECC_RSA     = (1UL << 2),
} CRYPTO_HW_MODULE;


/****** CRYPTO Event *****/
// wait current crypto HW finished
#define CSK_CRYPTO_EVENT_WAIT_DONE                (1UL << 0)
// callback event after crypto HW done
#define CSK_CRYPTO_EVENT_DONE                     (1UL << 1)
// wait if crypto HW busy
#define CSK_CRYPTO_EVENT_WAIT_BUSY                (1UL << 2)
// callback event after crypto procedure finished
#define CSK_CRYPTO_EVENT_FINISHED                 (1UL << 3)

// If this happens, a new start of crypto peripheral is needed.
#define CSK_CRYPTO_ERROR_HARDWARE                   (-8)
// if verify fail, return this value
#define CSK_CRYPTO_ERROR_VERIFY                     (-9)

typedef int32_t (*CSK_CRYPTO_SignalEvent_t) (uint32_t event, int32_t result, void* workspace);
// common functions
CSK_DRIVER_VERSION
CRYPTO_GetVersion (void);

void* CRYPTO0();

// this function should only be called once in one CPU
int32_t
CRYPTO_Initialize (void *res, CSK_CRYPTO_SignalEvent_t cb_event, void* workspace);

int32_t
CRYPTO_Uninitialize (void *res);

int32_t
CRYPTO_PowerControl (void* res, CRYPTO_HW_MODULE module, CSK_POWER_STATE state);

int32_t
CRYPTO_Control (void* res, uint32_t control, uint32_t arg0);

int32_t
CRYPTO_SWAP_Bytes(void *res, const uint32_t *data, uint32_t length, uint32_t *out);

// aes functions
int32_t
CRYPTO_AES_Encrypt (void* res, const uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest);

int32_t
CRYPTO_AES_Decrypt (void* res, const uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest);

int32_t
CRYPTO_ECB_Encrypt (void* res, const uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest);

int32_t
CRYPTO_ECB_Decrypt (void* res, const uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest);

int32_t
CRYPTO_CBC_Encrypt (void* res, const uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest);

int32_t
CRYPTO_CBC_Decrypt (void* res, const uint32_t * p_source,
                    uint32_t num_bytes, uint32_t * p_dest);

// hash functions, p_result only valid for last part, else set to NULL
int32_t
CRYPTO_Hash (void* res, const uint32_t * p_source,
            uint32_t num_bytes, uint32_t * p_result, uint32_t update);

#define CRYPTO_Hash_Update(res, src, num)      \
        CRYPTO_Hash(res, src, num, NULL, 1)
#define CRYPTO_Hash_Final(res, result)      \
        CRYPTO_Hash(res, NULL, 0, result, 1)

// hmac functions, call CRYPTO_Hash for more data
int32_t
CRYPTO_HMAC (void* res, const uint32_t * p_source, uint32_t num_bytes,
        const uint32_t * key, uint32_t key_bytes, uint32_t * p_result);

#define CRYPTO_HMAC_Update(res, src, num)      \
        CRYPTO_Hash(res, src, num, NULL, 1)
#define CRYPTO_HMAC_Final(res, result)      \
        CRYPTO_Hash(res, NULL, 0, result, 1)

int32_t
CRYPTO_Get_Hash(void* res, uint32_t * p_result);

// ecc functions
// calculate r = a op b mode n
enum crypto_mode_operate {
    CRYPTO_MOD_MULT = 1, CRYPTO_MOD_ADD = 2, CRYPTO_MOD_SUB = 3, CRYPTO_MOD_DIV = 4, CRYPTO_MOD_INV = 5, CRYPTO_MOD_MOD = 6, CRYPTO_MOD_EXP = 7
};
int32_t
CRYPTO_Mod_Operate(void *res, uint8_t op, uint8_t num_bytes, uint32_t *r, const uint32_t *a, const uint32_t *b, const uint32_t *n);

typedef struct _CRYPTO_ECC_CURVE {
    uint16_t param_len;
    uint16_t cofactor;
    uint16_t param_flag; // 1 for data with p', r^2; 0 for only curve data
    uint16_t resvered;
    uint8_t param[]; // total param buffer length = param_len * 8
} CRYPTO_ECC_CURVE;

// NIST prime curve
extern const CRYPTO_ECC_CURVE CRYPTO_ECC_CURVE_P192;
extern const CRYPTO_ECC_CURVE CRYPTO_ECC_CURVE_P224;
extern const CRYPTO_ECC_CURVE CRYPTO_ECC_CURVE_P256;
extern const CRYPTO_ECC_CURVE CRYPTO_ECC_CURVE_P384;
// brain pool curve
extern const CRYPTO_ECC_CURVE CRYPTO_ECC_CURVE_BP512R1;
extern const CRYPTO_ECC_CURVE CRYPTO_ECC_CURVE_BP512T1;
// guomi SM2 p256
extern const CRYPTO_ECC_CURVE CRYPTO_ECC_CURVE_SM2P256;

int32_t
CRYPTO_ECC_Build_Curve(void *res, CRYPTO_ECC_CURVE *curve);

int32_t
CRYPTO_ECC_Generate_Key(void *res, uint32_t seed, uint32_t *priv_key, uint32_t *public_key);

int32_t
CRYPTO_ECC_Verify_Key(void *res, const uint32_t *point);

// calculate result = P * k, if P == NULL, use G * k, if k == NULL, use efuse ecc key
int32_t
CRYPTO_ECC_Multiply(void *res, uint32_t *result, const uint32_t *p, const uint32_t *k);

// calculate result = P1 + P2
int32_t
CRYPTO_ECC_Add(void *res, uint32_t *result, const uint32_t *p1, const uint32_t *p2);

int32_t
CRYPTO_ECSDA_Verify_Signature(void *res, const uint32_t *hash, const uint32_t *pub_key, const uint32_t *sign);

int32_t
CRYPTO_Verify_Flash_Signature(void *res, const void *flash_zone, int sign_mode);

// RSA functions
// dest=source^public_key mode n
int32_t
CRYPTO_RSA_Encrypt (void* res, const uint32_t * p_source, uint32_t num_bytes,
                    uint32_t * p_dest, const uint32_t *n, uint32_t public_key);

// dest=source^priv_key mode n
int32_t
CRYPTO_RSA_Decrypt (void* res, const uint32_t * p_source, uint32_t num_bytes,
                    uint32_t * p_dest, uint32_t *out_bytes, const uint32_t *n, const uint32_t *priv_key);

// sign=hash^priv_key mode n
int32_t
CRYPTO_RSA_Sign_Signature(void *res, const uint32_t *hash, uint32_t hash_len, const uint32_t *n, const uint32_t *priv_key, uint32_t *sign);

// check hash=sign^public_key mode n
int32_t
CRYPTO_RSA_Verify_Signature(void *res, const uint32_t *hash, uint32_t hash_len, const uint32_t *n, uint32_t pub_key, const uint32_t *sign);

int32_t
CRYPTO_TKIP_Michael(void *res, const uint8_t *data, uint32_t data_len, uint8_t is_end, uint32_t *result);

int32_t
CRYPTO_IP_Checksum(void *res, const uint8_t *addr, uint16_t len, uint16_t *checksum);

#endif /* INCLUDE_DRIVER_DRIVER_CRYPTO_H_ */
