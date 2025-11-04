/*
 * crypto_rsa.c
 *
 *  Created on: 2023/5/24
 *      Author: LAPTOP-07
 */
#include "dbg_assert.h"
#include "crypto.h"

#include <string.h>
#include <stdlib.h>

enum rsa_operation
{
    RSA_OP_ENCRYPTION,
    RSA_OP_DECRYPTION,
    RSA_OP_SIGNATURE,
    RSA_OP_VERIFY,
    RSA_OP_EXP,
};

enum rsa_padding_operation
{
    RSA_PAD_INPUT,
    RSA_UNPAD_OUTPUT,
    RSA_PAD_VERIFY,
};

static const uint8_t rsa_sign_sha1_prefix[] = {
        0x14,0x04,0x00,0x05,0x1a,0x02,0x03,0x0e,0x2b,0x05,0x06,0x09,0x30,0x21,0x30
};
static const uint8_t rsa_sign_sha224_prefix[] = {
        0x1c,0x04,0x00,0x05,0x04,0x02,0x04,0x03,0x65,0x01,0x48,0x86,0x60,0x09,0x06,0x0d,0x30,0x2d,0x30
};
static const uint8_t rsa_sign_sha256_prefix[] = {
        0x20,0x04,0x00,0x05,0x01,0x02,0x04,0x03,0x65,0x01,0x48,0x86,0x60,0x09,0x06,0x0d,0x30,0x31,0x30
};
static const uint8_t rsa_sign_sha384_prefix[] = {
        0x30,0x04,0x00,0x05,0x02,0x02,0x04,0x03,0x65,0x01,0x48,0x86,0x60,0x09,0x06,0x0d,0x30,0x41,0x30
};
static const uint8_t rsa_sign_sha512_prefix[] = {
        0x40,0x04,0x00,0x05,0x03,0x02,0x04,0x03,0x65,0x01,0x48,0x86,0x60,0x09,0x06,0x0d,0x30,0x51,0x30
};

static const struct {const uint8_t * prefix; uint8_t length;}
rsa_sign_sha_prefixs[] = {
        {rsa_sign_sha1_prefix,   sizeof(rsa_sign_sha1_prefix)},
        {rsa_sign_sha224_prefix, sizeof(rsa_sign_sha224_prefix)},
        {rsa_sign_sha256_prefix, sizeof(rsa_sign_sha256_prefix)},
        {rsa_sign_sha384_prefix, sizeof(rsa_sign_sha384_prefix)},
        {rsa_sign_sha512_prefix, sizeof(rsa_sign_sha512_prefix)},
};

/**
 *****************************************************************************************
 * @brief RSA padding function type
 *
 * @param[in] crypto     crypto instance
 * @param[in] buff       RSA buffer
 * @param[in] rsa_len    RSA computation size in bytes
 * @param[in] in_out     Input or Output buffer (must be at least @p rsa_len long)
 * @param[in] len        Input vector length in bytes
 *
 * @return 0 - failure, len_out for unpad
 *****************************************************************************************
 */
typedef int (*rsa_padding_function)(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *in_out, uint32_t len);


static int rsa_pad_input_none(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *in, uint32_t len);
static int rsa_unpad_output_none(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *out, uint32_t len);
static int rsa_pad_verify_none(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *check, uint32_t len);
static int rsa_pad_input_pkcs(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *in, uint32_t len);
static int rsa_unpad_output_pkcs(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *out, uint32_t len);
static int rsa_pad_verify_pkcs(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *check, uint32_t len);
static int rsa_pad_input_pss(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *in, uint32_t len);
static int rsa_pad_verify_pss(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *check, uint32_t len);
static int rsa_pad_input_x931(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *in, uint32_t len);
static int rsa_unpad_output_x931(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *out, uint32_t len);
static int rsa_pad_verify_x931(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *check, uint32_t len);
static int rsa_pad_input_oaep(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *in, uint32_t len);
static int rsa_unpad_output_oaep(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *out, uint32_t len);

static const rsa_padding_function rsa_padding_ops[][3] =
{
    {rsa_pad_input_none, rsa_unpad_output_none, rsa_pad_verify_none}, // CSK_CRYPTO_RSA_PADDING_NONE
    {rsa_pad_input_pkcs, rsa_unpad_output_pkcs, rsa_pad_verify_pkcs}, // CSK_CRYPTO_RSA_PADDING_PKCS1
    {rsa_pad_input_pss,  NULL,                  rsa_pad_verify_pss},  // CSK_CRYPTO_RSA_PADDING_PSS
    {rsa_pad_input_x931, rsa_unpad_output_x931, rsa_pad_verify_x931}, // CSK_CRYPTO_RSA_PADDING_X931
    {rsa_pad_input_oaep, rsa_unpad_output_oaep, NULL},                // CSK_CRYPTO_RSA_PADDING_OAEP
};



static const uint8_t rsa_padding_length[] = {0, 11, 11, 2, 41};
/// HSU version
static uint32_t hsu_version = 0;
/// HSU revision register value
static uint32_t hsu_revision = 0;
/// HSU revision_1 register value
static uint32_t hsu_revision1 = 0;

/// Defined maximum input size for SHA and RSA operation
struct hsu_rsa_buf
{
    /// buffer used for RSA computation
    uint8_t rsa[(CRYPTO_RSA_MAX_LEN / 8) + 8];  // add 8bytes for mgf1 function
    /// buffer for padding
    uint8_t padding[(CRYPTO_RSA_MAX_LEN / 8)];
};

/// Intermediate buffer to regroup input vectors in a single one located in shared memory
/// Used for SHA and RSA operations
static struct hsu_rsa_buf hsu_buf;


/**
 *****************************************************************************************
 * @brief Copy input vector for RSA computation
 *
 * @param[in] in         Input vector
 * @param[in] len        Input vector length in bytes
 * @param[in] rsa_len    RSA computation size in bytes
 * @param[in] out        Output buffer (must be at least @p rsa_len long)
 * @param[in] swap_byte  Whether buffer should be copied using the same
 *                       endianness or the other one
 *****************************************************************************************
 */
static void hsu_rsa_copy_vector(const uint8_t *in, int len, uint32_t rsa_len,
                                uint8_t *out, bool swap_byte)
{
    if (rsa_len > len)
    {
        // If len < rsa_len then it is necessary an input vector.
        // pad with zero on LSB as HSU expect little endian buffer.
        int zero_len = rsa_len - len;
        memset(out+len, 0, zero_len);
    }

    if (swap_byte)
    {
        in += len - 1;
        while (len > 0)
        {
            *out++ = *in--;
            len--;
        }
    }
    else
    {
        memcpy(out, in, len);
    }
}

static int hsu_rsa_compare_vector(const uint8_t *buff, int len,
                                uint8_t *check, bool swap_byte)
{
    if(len == 0)
        return 0;

    if (swap_byte)
    {
        buff += len - 1;
        while (len > 0)
        {
            if(*check++ != *buff--)
                return 0;
            len--;
        }
    }
    else
    {
        if(memcpy(check, buff, len) != 0)
            return 0;
    }
    return 1;
}

// need 4 more byte after the buff
static int rsa_pkcs1_mgf1(CRYPTO_RESOURCES *crypto, uint32_t *buff, uint32_t len,
        uint32_t *mask, uint32_t mask_len)
{
    uint32_t i, outlen = 0;
    int hlen = rsa_sign_sha_prefixs[crypto->sha_info->mode-1].prefix[0];
    uint8_t *p = (uint8_t*)buff;

    p[len] = 0;p[len+1] = 0;p[len+2] = 0;
    /* step 4 */
    for(i=0; outlen<mask_len; i++)
    {
        /* step 4b: T =T || hash(mgfSeed || D) */
        p[len+3] = i;
        CRYPTO_Hash((void*)crypto, buff, len + 4, mask+outlen/4, 0);
        outlen += hlen;
    }

    return 1;
}


// CSK_CRYPTO_RSA_PADDING_NONE
static int rsa_pad_input_none(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *in, uint32_t len)
{
    memset(buff + len, 0, rsa_len - len);
    hsu_rsa_copy_vector(in, len, len, buff, !crypto->info->little_endian);
    return 1;
}

static int rsa_unpad_output_none(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *out, uint32_t len)
{
    len = rsa_len;

    while(buff[--len] == 0  && len > 0);
    len ++;

    hsu_rsa_copy_vector(buff, len, len, out, !crypto->info->little_endian);
    return len;
}

static int rsa_pad_verify_none(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *check, uint32_t len)
{
    len = rsa_len;

    while(buff[--len] == 0  && len > 0);
    len ++;

    if(len != 0)
    {
        len = hsu_rsa_compare_vector(buff, len, check, !crypto->info->little_endian);
    }
    return len;
}

// CSK_CRYPTO_RSA_PADDING_PKCS1
static int rsa_pad_input_pkcs(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *in, uint32_t len)
{
    // type 2
    if(crypto->rsa_info->operation == RSA_OP_ENCRYPTION)
    {
        buff[--rsa_len] = 0;
        buff[--rsa_len] = 2;
        while(rsa_len > len)
        {
            buff[--rsa_len] = rand()& 0xff;
            while(buff[rsa_len] == 0)
                buff[rsa_len] = rand()& 0xff;
        }
        buff[len] = 0;
    }
    // type 1
    else if(crypto->rsa_info->operation == RSA_OP_SIGNATURE)
    {
        uint8_t prefix_len = rsa_sign_sha_prefixs[crypto->sha_info->mode-1].length;
        memcpy(buff+len,
            rsa_sign_sha_prefixs[crypto->sha_info->mode-1].prefix, prefix_len);

        buff[rsa_len - 1] = 0;
        buff[rsa_len - 2] = 1;
        memset(buff+len+prefix_len, 0xff, rsa_len-len-prefix_len-2);
        buff[len+prefix_len] = 0;
    }

    hsu_rsa_copy_vector(in, len, len, buff, !crypto->info->little_endian);
    return 1;
}

static int rsa_unpad_output_pkcs(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *out, uint32_t len)
{
    // type 2
    len = rsa_len - 2;
    while(buff[--len] != 0  && len > 0);
    if(buff[len] != 0)
        len = 0;

    hsu_rsa_copy_vector(buff, len, len, out, !crypto->info->little_endian);
    return len;
}

static int rsa_pad_verify_pkcs(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *check, uint32_t len)
{
    // type 1
    uint8_t prefix_len = rsa_sign_sha_prefixs[crypto->sha_info->mode-1].length;

    len = rsa_len - 2;
    while(buff[--len] != 0 && len > 1);
    if(buff[len] != 0)
        len = 0;
    else
    {
        len -= prefix_len;
        if(memcmp(buff+len,
            rsa_sign_sha_prefixs[crypto->sha_info->mode-1].prefix,
            prefix_len) != 0)
            len = 0;
        if(len != rsa_sign_sha_prefixs[crypto->sha_info->mode-1].prefix[0])
            len = 0;

        if(len != 0)
        {
            len = hsu_rsa_compare_vector(buff, len, check, !crypto->info->little_endian);
        }
    }
    return len;
}

// CSK_CRYPTO_RSA_PADDING_PSS
static int rsa_pad_input_pss(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *in, uint32_t len)
{
    int hlen = rsa_sign_sha_prefixs[crypto->sha_info->mode-1].prefix[0];
    int i, slen, mask_len;

    uint8_t *Mp, *DB, *EM, *salt, *H;

    slen = hlen;
    mask_len = rsa_len - hlen - 1;
    if(slen + 1 > mask_len)
        slen = mask_len - 1;

    // M' = PAD(8 zeros) | mHash | salt
    Mp = buff + hlen;
    salt = Mp + 8 + hlen;

    memset(Mp, 0, 8);
    memcpy(Mp+8, in, len);

    for(i=0;i<slen;i++)
        salt[i] = rand()&0xff;
    memcpy(hsu_buf.padding + rsa_len - slen, salt, slen);
    salt = hsu_buf.padding + rsa_len - slen;

    H = hsu_buf.padding;
    CRYPTO_Hash((void*)crypto, (uint32_t*)Mp, slen + len + 8, (uint32_t*)H, 0);

    rsa_pkcs1_mgf1(crypto, (uint32_t*)H, hlen, (uint32_t*)buff, rsa_len - hlen - 1);

    buff[mask_len-slen-1] ^= 1;

    for(i=0;i<slen;i++)
        buff[mask_len-slen+i] ^= salt[i];

    // copy H
    memcpy(buff + mask_len, H, hlen);
    buff[rsa_len - 1] = 0xbc;

    uint8_t msb_n = crypto->rsa_info->msb_n;
    uint8_t mask = 0x80;

    if(msb_n == 0)
        buff[0] = 0;
    else
    {
        while(1)
        {
            buff[0] &= ~mask;
            if(msb_n & mask)
                break;
            mask >>= 1;
        }
    }

    // swap byte if need
    if(!crypto->info->little_endian)
        CRYPTO_SWAP_Bytes((void*)crypto, (uint32_t*)buff, rsa_len, (uint32_t*)buff);

    return 1;
}

static int rsa_pad_verify_pss(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *check, uint32_t len)
{
    int hlen = rsa_sign_sha_prefixs[crypto->sha_info->mode-1].prefix[0];
    int i, spos, slen, mask_len;

    uint8_t *Mp, *DB, *EM, *salt, *H, *DBMask;

    // swap byte if need
    if(!crypto->info->little_endian)
        CRYPTO_SWAP_Bytes((void*)crypto, (uint32_t*)buff, rsa_len, (uint32_t*)buff);

    if(buff[rsa_len - 1] != 0xbc)
        return 0;

    uint8_t msb_n = crypto->rsa_info->msb_n;
    uint8_t mask = 0x80;

    if(msb_n == 0 && buff[0] != 0)
        return 0;
    else
    {
        while(1)
        {
            if(buff[0] & mask)
                return 0;
            if(msb_n & mask)
                break;
            mask >>= 1;
        }
    }


    mask_len = rsa_len - hlen - 1;

    // copy H for 32bit align
    H = hsu_buf.padding + rsa_len - hlen;
    memcpy(H, buff + mask_len, hlen);

    DBMask = hsu_buf.padding;
    rsa_pkcs1_mgf1(crypto, (uint32_t*)H, hlen, (uint32_t*)DBMask, mask_len);

    for(i=0; i<mask_len; i++)
    {
        buff[i] ^= DBMask[i];
    }

    mask = 0x80;
    while(1)
    {
        buff[0] &= ~mask;
        if(msb_n & mask)
            break;
        mask >>= 1;
    }
    for(i=0; i<mask_len; i++)
    {
        if(buff[i] != 0)
            break;
    }
    spos = i;
    if(buff[spos] != 1)
        return 0;

    slen = mask_len - spos - 1;
    memmove(buff + hlen + 8, buff + spos + 1, slen);
    memset(buff, 0, 8);
    memcpy(buff + 8, check, hlen);

    CRYPTO_Hash((void*)crypto, (uint32_t*)buff, slen + hlen + 8, (uint32_t*)buff, 0);

    if(memcmp(buff, H, hlen)!=0)
        return 0;
    else
        return 1;
}

// CSK_CRYPTO_RSA_PADDING_X931
static int rsa_pad_input_x931(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *in, uint32_t len)
{
    int pad_len = rsa_len - len;

    if(pad_len == 2)
    {
        buff[rsa_len - 1] = 0x6A;
    }
    else
    {
        buff[rsa_len - 1] = 0x6B;
        memset(buff+len+2, 0xBB, pad_len-3);
        buff[len+1] = 0xBA;
    }
    buff[0] = 0xcc;
    buff ++;
    hsu_rsa_copy_vector(in, len, len, buff, !crypto->info->little_endian);
    return 1;
}

static int rsa_unpad_output_x931(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *out, uint32_t len)
{
    len = rsa_len;

    if(buff[0] != 0xCC)
    {
        len = 0; // error
    }
    else if(buff[len-1] == 0x6A)
    {
        len -= 2;
        buff += 1;
    }
    else if(buff[len-1] == 0x6B)
    {
        len --;
        while(buff[--len] == 0xBB  && len > 0);
        if(buff[len] == 0xBA)
        {
            len --;
            buff += 1;
        }
        else
            len = 0; // error
    }
    else
        len = 0; // error

    hsu_rsa_copy_vector(buff, len, len, out, !crypto->info->little_endian);
    return len;

}

static int rsa_pad_verify_x931(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *check, uint32_t len)
{
    len = rsa_len;

    if(buff[0] != 0xCC)
    {
        len = 0; // error
    }
    else if(buff[len-1] == 0x6A)
    {
        len -= 2;
        buff += 1;
    }
    else if(buff[len-1] == 0x6B)
    {
        len --;
        while(buff[--len] == 0xBB  && len > 0);
        if(buff[len] == 0xBA)
        {
            len --;
            buff += 1;
        }
        else
            len = 0; // error
    }
    else
        len = 0; // error

    if(len != 0)
    {
        len = hsu_rsa_compare_vector(buff, len, check, !crypto->info->little_endian);
    }
    return len;
}

// CSK_CRYPTO_RSA_PADDING_OAEP
static int rsa_pad_input_oaep(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *in, uint32_t len)
{
    int hlen = rsa_sign_sha_prefixs[crypto->sha_info->mode-1].prefix[0];
    // check label
    if(crypto->rsa_info->oaep_label == NULL || strlen(crypto->rsa_info->oaep_label) == 0)
        return 0;

    uint8_t *db, *seed, *mask;
    int i;
    int dblen;

    mask = hsu_buf.padding;

    /* step 2b: check KLen > nLen - 2 HLen - 2 */
    if(rsa_len < 2 * hlen + 1)
        return 0;
    if(len > rsa_len - 2 * hlen - 1)
        return 0;

    /* step 3i: EM = 00000000 || maskedMGF || maskedDB */
    seed = buff;
    db = seed + hlen + 4; // skip 4 bytes for mgf1 usage
    dblen = rsa_len - hlen - 1;

    /* step 3a: hash the additional input */
    CRYPTO_Hash((void*)crypto, (uint32_t*)crypto->rsa_info->oaep_label, strlen(crypto->rsa_info->oaep_label),
            (uint32_t*)db, 0);
    /* step 3b: zero bytes array of length nLen - KLen - 2 HLen -2 */
    memset(db + hlen, 0, rsa_len - len - 2 * hlen - 1);
    /* step 3c: DB = HA || PS || 00000001 || K */
    db[rsa_len - len - hlen - 1] = 0x01;
    memcpy(db + rsa_len - len - hlen, in, len);
    /* step 3d: generate random byte string */
    for(i=0;i<hlen;i++)
        seed[i] = rand()&0xff;

    /* step 3e: dbMask = MGF(mgfSeed, nLen - HLen - 1) */
    rsa_pkcs1_mgf1(crypto, (uint32_t*)seed, hlen, (uint32_t*)mask, dblen);

    /* step 3f: maskedDB = DB XOR dbMask */
    for(i=0; i<dblen; i++)
        db[i] ^= mask[i];

    /* step 3g: mgfSeed = MGF(maskedDB, HLen) */
    rsa_pkcs1_mgf1(crypto, (uint32_t*)db, dblen, (uint32_t*)mask, hlen);

    /* stepo 3h: maskedMGFSeed = mgfSeed XOR mgfSeedMask */
    for(i=hlen; i>0; i--)
        seed[i] = seed[i-1]^mask[i-1];

    // insert first zero
    memcpy(buff + hlen + 1, buff + hlen + 4, dblen);
    buff[0] = 0;

    // swap byte if need
    if(!crypto->info->little_endian)
        CRYPTO_SWAP_Bytes((void*)crypto, (uint32_t*)buff, rsa_len, (uint32_t*)buff);
    return 1;
}

static int rsa_unpad_output_oaep(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
        uint8_t *out, uint32_t len)
{

    int hlen = rsa_sign_sha_prefixs[crypto->sha_info->mode-1].prefix[0];
    // check label
    if(crypto->rsa_info->oaep_label == NULL || strlen(crypto->rsa_info->oaep_label) == 0)
        return 0;

    uint8_t *db, *seed, *mask;
    int i;
    int dblen;

    mask = hsu_buf.padding;

    if(rsa_len < 2 * hlen + 1)
        return 0;

    seed = buff;
    db = seed + hlen + 4;
    dblen = rsa_len - hlen - 1;

    // swap byte if need
    if(!crypto->info->little_endian)
        CRYPTO_SWAP_Bytes((void*)crypto, (uint32_t*)buff, rsa_len, (uint32_t*)buff);

    if(buff[0] != 0)
        return 0;

    // move data to 32bit aligned
    memmove(seed, buff+1, hlen);
    memmove(db, buff+hlen+1, dblen);

    rsa_pkcs1_mgf1(crypto, (uint32_t*)db, dblen, (uint32_t*)mask, hlen);
    for(i=0; i<hlen; i++)
        seed[i] ^= mask[i];

    rsa_pkcs1_mgf1(crypto, (uint32_t*)seed, hlen, (uint32_t*)mask, dblen);
    for(i=0; i<dblen; i++)
        db[i] ^= mask[i];

    // check label
    CRYPTO_Hash((void*)crypto, (uint32_t*)crypto->rsa_info->oaep_label, strlen(crypto->rsa_info->oaep_label),
            (uint32_t*)mask, 0);
    if(memcmp(mask, db, hlen) != 0)
        return 0;

    /* check PS and 1 DB = HA || PS || 00000001 || K */
    i = hlen-1;
    while(db[++i]==0);
    if(db[i] != 1)
        return 0;

    len = dblen - i - 1;
    memcpy(out, &(db[i+1]), len);

    return len;
}


static void hsu_rsa_pad_input(CRYPTO_RESOURCES *crypto, const uint8_t *in, uint32_t len, uint32_t rsa_len,
                                uint8_t *buff)
{
    uint8_t padding_mode = crypto->rsa_info->padding_mode;

    if(rsa_padding_ops[padding_mode][RSA_PAD_INPUT] != NULL)
        rsa_padding_ops[padding_mode][RSA_PAD_INPUT](crypto, buff, rsa_len, (uint8_t *)in, len);
    else
        rsa_pad_input_none(crypto, buff, rsa_len, (uint8_t *)in, len);

}

static void hsu_rsa_unpad_output(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
                                uint8_t *out, uint32_t *plen)
{
    uint8_t padding_mode = crypto->rsa_info->padding_mode;
    uint32_t len = *plen;

    if(rsa_padding_ops[padding_mode][RSA_UNPAD_OUTPUT] != NULL)
        len = rsa_padding_ops[padding_mode][RSA_UNPAD_OUTPUT](crypto, buff, rsa_len, out, len);
    else
        len = rsa_unpad_output_none(crypto, buff, rsa_len, out, len);


    if(plen)
        *plen = len;
}


static int hsu_rsa_check_output(CRYPTO_RESOURCES *crypto, uint8_t *buff, uint32_t rsa_len,
                                uint8_t *check)
{
    uint8_t padding_mode = crypto->rsa_info->padding_mode;

    if(rsa_padding_ops[padding_mode][RSA_PAD_VERIFY] != NULL)
        return rsa_padding_ops[padding_mode][RSA_PAD_VERIFY](crypto, buff, rsa_len, check, rsa_len);
    else
        return 0;


}

/**
 *****************************************************************************************
 * @brief Generic function for RSA computation
 *
 * Compute the RSA (i.e. modular exponentiation) for the provide input buffer.
 * out = input[0] ^ input[1] % input[2]
 *
 * The input buffers are padded with 0 on MSB if shorter than @p rsa_len bytes.
 * Caller must ensure that HSU supports the requested mode and that the output
 * buffer is big enough.
 *
 * @param[in]  hsu_rsa_mode   Mode to configure for HSU
 * @param[in]  rsa_len        Size, in bytes, of the RSA computation.
 *                            Must be aligned with @p hsu_rsa_mode
 * @param[in]  little_endian  Whether input buffer are in little endian or big endian
 *                            format.
 *                            Output buffer will be written using the same endianness.
 * @param[in]  input          Table of input buffers.
 *                            [0]: message
 *                            [1]: exponent
 *                            [2]: modulus
 * @param[in]  input_len      Table of input buffers size, in bytes.
 *
 * @return CSK_DRIVER_OK if the RSA was correctly computed, false otherwise
 *****************************************************************************************
 */
static int32_t hsu_rsa_x(CRYPTO_RESOURCES *crypto, uint32_t hsu_rsa_mode, uint32_t rsa_len,
                      const uint8_t *input[3], const uint32_t input_len[3])
{
    int32_t res;
    LOGD("[%s]: operation=%d\r\n", __func__,
            crypto->rsa_info->operation);


    //hsu_length_set(rsa_len);
    crypto->hsu_reg->REG_LENGTH.all = rsa_len;
    //hsu_source_addr_set(CPU2HW(hsu_buf.rsa));
    crypto->hsu_reg->REG_SOURCE_ADDR.all = (uint32_t)hsu_buf.rsa;
    // destination is written only after all inputs are read so it's ok to re-use
    // the same buffer
    //hsu_destination_addr_set(CPU2HW(hsu_buf.rsa));
    crypto->hsu_reg->REG_DESTINATION_ADDR.all = (uint32_t)hsu_buf.rsa;
    crypto->hsu_reg->REG_IRQ_CTRL_EN.bit.CRYPTO_IRQ_EN = 0;

    //hsu_status_clear_set(HSU_DONE_CLEAR_BIT);
    crypto->hsu_reg->REG_STATUS_CLEAR.bit.DONE_CLEAR = 1;
    //hsu_control_set(hsu_rsa_mode | HSU_FIRST_BUFFER_BIT);
    crypto->hsu_reg->REG_CONTROL.bit.FIRST_BUFFER = 1;
    crypto->hsu_reg->REG_CONTROL.bit.LAST_BUFFER = 0;
    crypto->hsu_reg->REG_CONTROL.bit.MODE = hsu_rsa_mode;
    crypto->hsu_reg->REG_CONTROL.bit.START = 1;
    CRYPTO_HSU_WAIT_DONE(RSA);

    hsu_rsa_copy_vector(input[1], input_len[1], rsa_len, hsu_buf.rsa, !crypto->info->little_endian);
    //hsu_status_clear_set(HSU_DONE_CLEAR_BIT);
    crypto->hsu_reg->REG_STATUS_CLEAR.bit.DONE_CLEAR = 1;
    //hsu_control_set(hsu_rsa_mode);
    crypto->hsu_reg->REG_CONTROL.bit.FIRST_BUFFER = 0;
    crypto->hsu_reg->REG_CONTROL.bit.START = 1;
    CRYPTO_HSU_WAIT_DONE(RSA);

    hsu_rsa_copy_vector(input[2], input_len[2], rsa_len, hsu_buf.rsa, !crypto->info->little_endian);
    //hsu_status_clear_set(HSU_DONE_CLEAR_BIT);
    crypto->hsu_reg->REG_STATUS_CLEAR.bit.DONE_CLEAR = 1;
    //hsu_enable_crypto_irq();
    crypto->hsu_reg->REG_IRQ_CTRL_EN.bit.CRYPTO_IRQ_EN = 1;

    //hsu_control_set(hsu_rsa_mode | HSU_LAST_BUFFER_BIT);
    crypto->hsu_reg->REG_CONTROL.bit.LAST_BUFFER = 1;
    crypto->hsu_reg->REG_CONTROL.bit.START = 1;

    res = crypto->info->cb_event(CSK_CRYPTO_EVENT_WAIT_DONE, CSK_DRIVER_OK, NULL);

    return res;
}


// RSA functions
// dest=source^public_key mode n
int32_t
CRYPTO_RSA_Encrypt (void* res, const uint32_t * p_source, uint32_t num_bytes,
                    uint32_t * p_dest, const uint32_t *n, uint32_t public_key)
{
    CHECK_RESOURCES(res);
    if(p_source==NULL || p_dest == NULL || n == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    int rsa_mode = crypto->rsa_info->mode;
    uint32_t rsa_len = (1<<rsa_mode)/64;

    int max_len = rsa_len - rsa_padding_length[crypto->rsa_info->padding_mode];

    if(num_bytes > max_len)
        return CSK_DRIVER_ERROR_PARAMETER;

    if(rsa_padding_ops[crypto->rsa_info->padding_mode][RSA_PAD_INPUT] == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    crypto->rsa_info->operation = RSA_OP_ENCRYPTION;

    const uint8_t *input[3] = {(uint8_t*)p_source, (uint8_t*)&public_key, (uint8_t*)n};
    const uint32_t input_len[3] = {num_bytes, sizeof(uint32_t), rsa_len};

    hsu_rsa_pad_input(crypto, input[0], input_len[0], rsa_len, hsu_buf.rsa);

    hsu_rsa_x(crypto, rsa_mode, rsa_len, input, input_len);

    hsu_rsa_copy_vector(hsu_buf.rsa, rsa_len, rsa_len, (uint8_t*)p_dest, !crypto->info->little_endian);

    return CSK_DRIVER_OK;
}

// dest=source^priv_key mode n
int32_t
CRYPTO_RSA_Decrypt (void* res, const uint32_t * p_source, uint32_t num_bytes,
                    uint32_t * p_dest, uint32_t *out_bytes, const uint32_t *n, const uint32_t *priv_key)
{
    CHECK_RESOURCES(res);
    if(p_source==NULL || p_dest == NULL || n == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    int rsa_mode = crypto->rsa_info->mode;
    uint32_t rsa_len = (1<<rsa_mode)/64;

    if(num_bytes != rsa_len)
        return CSK_DRIVER_ERROR_PARAMETER;

    if(rsa_padding_ops[crypto->rsa_info->padding_mode][RSA_UNPAD_OUTPUT] == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    crypto->rsa_info->operation = RSA_OP_DECRYPTION;

    const uint8_t *input[3] = {(uint8_t*)p_source, (uint8_t*)priv_key, (uint8_t*)n};
    const uint32_t input_len[3] = {rsa_len, rsa_len, rsa_len};

    hsu_rsa_copy_vector(input[0], input_len[0], rsa_len, hsu_buf.rsa, !crypto->info->little_endian);

    hsu_rsa_x(crypto, rsa_mode, rsa_len, input, input_len);

    hsu_rsa_unpad_output(crypto, hsu_buf.rsa, rsa_len, (uint8_t*)p_dest, out_bytes);

    return CSK_DRIVER_OK;
}

// sign=hash^priv_key mode n
int32_t
CRYPTO_RSA_Sign_Signature(void *res, const uint32_t *hash, uint32_t hash_len, const uint32_t *n, const uint32_t *priv_key, uint32_t *sign)
{
    CHECK_RESOURCES(res);
    if(hash==NULL || sign == NULL || n == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;

    int rsa_mode = crypto->rsa_info->mode;
    uint32_t rsa_len = (1<<rsa_mode)/64;

    int max_len = rsa_len - rsa_padding_length[crypto->rsa_info->padding_mode];

    if(hash_len > max_len)
        return CSK_DRIVER_ERROR_PARAMETER;

    if(rsa_padding_ops[crypto->rsa_info->padding_mode][RSA_PAD_INPUT] == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    crypto->rsa_info->operation = RSA_OP_SIGNATURE;
    crypto->rsa_info->msb_n = n[0];

    const uint8_t *input[3] = {(uint8_t*)hash, (uint8_t*)priv_key, (uint8_t*)n};
    const uint32_t input_len[3] = {hash_len, rsa_len, rsa_len};

    hsu_rsa_pad_input(crypto, input[0], input_len[0], rsa_len, hsu_buf.rsa);

    hsu_rsa_x(crypto, rsa_mode, rsa_len, input, input_len);

    hsu_rsa_copy_vector(hsu_buf.rsa, rsa_len, rsa_len, (uint8_t*)sign, !crypto->info->little_endian);

    return CSK_DRIVER_OK;
}


// check hash=sign^public_key mode n
int32_t
CRYPTO_RSA_Verify_Signature(void *res, const uint32_t *hash, uint32_t hash_len, const uint32_t *n, uint32_t pub_key, const uint32_t *sign)
{
    CHECK_RESOURCES(res);
    if(hash==NULL || sign == NULL || n == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    CRYPTO_RESOURCES* crypto = (CRYPTO_RESOURCES*)res;
    int32_t result = CSK_DRIVER_OK;

    int rsa_mode = crypto->rsa_info->mode;
    uint32_t rsa_len = (1<<rsa_mode)/64;

    if(rsa_padding_ops[crypto->rsa_info->padding_mode][RSA_PAD_VERIFY] == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    crypto->rsa_info->operation = RSA_OP_VERIFY;
    crypto->rsa_info->msb_n = n[0];

    const uint8_t *input[3] = {(uint8_t*)sign, (uint8_t*)&pub_key, (uint8_t*)n};
    const uint32_t input_len[3] = {rsa_len, sizeof(uint32_t), rsa_len};

    hsu_rsa_copy_vector(input[0], input_len[0], rsa_len, hsu_buf.rsa, !crypto->info->little_endian);

    hsu_rsa_x(crypto, rsa_mode, rsa_len, input, input_len);

    if(!hsu_rsa_check_output(crypto, hsu_buf.rsa, rsa_len, (uint8_t*)hash))
        result = CSK_CRYPTO_ERROR_VERIFY;

    return result;
}



int32_t crypto_rsa_set_mode(CRYPTO_RESOURCES *crypto, uint32_t mode)
{
    if(CRYPTO_RSA_MODE_1024==mode && !CRYPTO_HSU_SUPPORT(RSA_1024))
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    if(CRYPTO_RSA_MODE_2048==mode && !CRYPTO_HSU_SUPPORT(RSA_2048))
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    if(CRYPTO_RSA_MODE_4096==mode && !CRYPTO_HSU_SUPPORT(RSA_4096))
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    crypto->rsa_info->mode = mode + HSU_MODE_RSA_1024 - 1;
    return CSK_DRIVER_OK;
}

int32_t crypto_rsa_set_padding_mode(CRYPTO_RESOURCES *crypto, uint32_t mode)
{
    if(mode > CSK_CRYPTO_RSA_PADDING_OAEP)
        return CSK_DRIVER_ERROR_PARAMETER;
    crypto->rsa_info->padding_mode = mode;
    return CSK_DRIVER_OK;
}

int32_t crypto_rsa_set_padding_label(CRYPTO_RESOURCES *crypto, char *label)
{
    if(label == NULL || strlen(label) == 0)
        return CSK_DRIVER_ERROR_PARAMETER;
    crypto->rsa_info->oaep_label = label;
    return CSK_DRIVER_OK;
}

int32_t crypto_mod_exp(CRYPTO_RESOURCES *crypto, uint32_t *res, int *res_len, const uint32_t *val,
                       int val_len, const uint32_t *exponent, int exponent_len, const uint32_t *modulus, int modulus_len)
{
    int rsa_mode, rsa_len = modulus_len;
    const uint8_t *input[3] = {(uint8_t*)val, (uint8_t*)exponent, (uint8_t*)modulus};
    const uint32_t input_len[3] = {val_len, exponent_len, modulus_len};

    if (val_len > modulus_len)
        rsa_len = val_len;

    if (exponent_len > rsa_len)
        rsa_len = exponent_len;

    if (rsa_len <= 128)
    {
        int mod;

        // RSA mode 768/512/256 are only supported if 1024 is supported
        if (!CRYPTO_HSU_SUPPORT(RSA_1024))
            return CSK_DRIVER_ERROR_UNSUPPORTED;

        rsa_len = (rsa_len + 31) & ~31;

        if ((rsa_len == 32) && CRYPTO_HSU_SUPPORT(RSA_256))
            rsa_mode = HSU_MODE_RSA_256;
        else if ((rsa_len <= 64) && CRYPTO_HSU_SUPPORT(RSA_512))
        {
            rsa_len = 64;
            rsa_mode = HSU_MODE_RSA_512;
        }
        else if ((rsa_len <= 96) && CRYPTO_HSU_SUPPORT(RSA_768))
        {
            rsa_len = 96;
            rsa_mode = HSU_MODE_RSA_768;
        }
        else
        {
            rsa_len = 128;
            rsa_mode = HSU_MODE_RSA_1024;
        }
    }
    else if (rsa_len <= 256)
    {
        if (!CRYPTO_HSU_SUPPORT(RSA_2048))
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        rsa_len = 256;
        rsa_mode = HSU_MODE_RSA_2048;
    }
    else if (rsa_len <= 512)
    {
        if (!CRYPTO_HSU_SUPPORT(RSA_4096)) // (crypto->hsu_reg->REG_REVISION.bit.RSA_4096)
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        rsa_len = 512;
        rsa_mode = HSU_MODE_RSA_4096;
    }
    else
    {
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    crypto->rsa_info->operation = RSA_OP_EXP;
    if (res_len)
        *res_len = rsa_len;

    hsu_rsa_copy_vector(input[0], input_len[0], rsa_len, hsu_buf.rsa, !crypto->info->little_endian);

    hsu_rsa_x(crypto, rsa_mode, rsa_len, input, input_len);

    hsu_rsa_copy_vector(hsu_buf.rsa, rsa_len, rsa_len, (uint8_t*)res, !crypto->info->little_endian);

    return CSK_DRIVER_OK;
}

void crypto_rsa_irq_handler(CRYPTO_RESOURCES *crypto)
{
    uint32_t res = CSK_DRIVER_OK;
    LOGD("[%s]: operation=%d\r\n", __func__,
            crypto->rsa_info->operation);

    // call user callback
    if(crypto->info->cb_event)
        crypto->info->cb_event(CSK_CRYPTO_EVENT_DONE, res, (void*)crypto);
}
