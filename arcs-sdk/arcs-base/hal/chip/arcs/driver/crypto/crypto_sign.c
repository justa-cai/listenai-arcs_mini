/*
 * crypto_sign.c
 *
 *  Created on: 2024/5/28
 *      Author: LAPTOP-07
 */

#include "dbg_assert.h"
#include "crypto.h"
#include "ota.h"
#include "Driver_CRYPTO.h"
#include <string.h>
#include <stdlib.h>

#define CRYPTO_SIGN_BUFF_SIZE   (64)  // must >= sizeof(ls_ota_header_t)

#if 1
const int sign_size[] = {0, 4, 32, 128, 512};

int32_t
check_public_key(void *pCrypto_Handler, uint8_t *pub_key, uint32_t *buff, int size)
{
    int32_t res = CSK_DRIVER_OK;

    do
    {
        res = CRYPTO_Hash(pCrypto_Handler, (uint32_t*)pub_key, size, buff, 0);

        if(res != CSK_DRIVER_OK)
        {
            break;
        }

        uint32_t *efuse_icv = (uint32_t *)&(IP_EFUSE_CTRL->REG_AUTO_LOAD_40.all);
        for(int i=0; i<256/32; i++)
        {
            if(efuse_icv[i] != buff[i])
            {
                res = CSK_CRYPTO_ERROR_VERIFY;
                break;
            }
        }

    }while(0);

    return res;
}

int32_t
CRYPTO_Verify_Flash_Signature(void *pCrypto_Handler, const void *flash_zone, int sign_mode)
{
    int32_t res = CSK_DRIVER_OK;
    const ls_ota_header_t *hdr = (const ls_ota_header_t*)flash_zone;
    uint32_t buff[CRYPTO_SIGN_BUFF_SIZE];
    uint32_t len, size;

    do{
        CRYPTO_Control(pCrypto_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA256);


        if(sign_mode!=OTA_SIGN_SHA256)
        {
            res = check_public_key(pCrypto_Handler, (uint8_t*)(hdr->sign+sign_size[sign_mode]/8), buff, sign_size[sign_mode]/2);
            if(res != CSK_DRIVER_OK)
            {
                break;
            }
        }

        // calculate hash
        memcpy(buff, hdr, CRYPTO_SIGN_BUFF_SIZE);
        /// the sign is based on the flag 0xffffffff
        buff[0] = 0xffffffff;
        buff[6] &= ~(OTA_SIGN_MASK|OTA_ENC_MASK);
        buff[15] = 0; // set crc32 to 0
        /// the sign is base on zeros with sign data
        res = CRYPTO_Hash(pCrypto_Handler, buff, sizeof(ls_ota_header_t), NULL, 0);

        len = 0;
        memset(buff, 0, sizeof(buff));
        while(len<sign_size[sign_mode])
        {
            size = sign_size[sign_mode] - len;
            if(size < CRYPTO_SIGN_BUFF_SIZE)
            {
                memcpy(((uint8_t*)buff)+size, (((uint8_t*)hdr)+sizeof(ls_ota_header_t)+len+size), CRYPTO_SIGN_BUFF_SIZE-size);

            }
            res = CRYPTO_Hash(pCrypto_Handler, (uint32_t*)buff, CRYPTO_SIGN_BUFF_SIZE, NULL, 1);

            len += CRYPTO_SIGN_BUFF_SIZE;
        }

        /// calculate the image data
        len += sizeof(ls_ota_header_t);
        while(len < hdr->size && res == CSK_DRIVER_OK)
        {
            size = hdr->size - len;
            if(size > CRYPTO_MAX_PACKAGE_SIZE)
            {
                size = CRYPTO_MAX_PACKAGE_SIZE;
                res = CRYPTO_Hash(pCrypto_Handler, (uint32_t*)(((uint8_t*)hdr)+len), size, NULL, 1);
            }
            else
            {
                res = CRYPTO_Hash(pCrypto_Handler, (uint32_t*)(((uint8_t*)hdr)+len), size, buff, 1);
            }

            len += size;
        };

        if(sign_mode==OTA_SIGN_SHA256)
        {
            if(memcmp(buff, hdr->sign, sign_size[sign_mode])!=0)
                res = CSK_CRYPTO_ERROR_VERIFY;
            break;
        }
        else if(sign_mode==OTA_SIGN_ECSDA256)
        {
            CRYPTO_Control(pCrypto_Handler, CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P256);

            res = CRYPTO_ECSDA_Verify_Signature(pCrypto_Handler, buff, (uint32_t*)(hdr->sign+sign_size[sign_mode]/8), (uint32_t*)hdr->sign);
            break;
        }
        else if(sign_mode==OTA_SIGN_RSA2048)
        {
            uint32_t crypto_rsa_e_bigendian = 0x01000100;
            CRYPTO_Control(pCrypto_Handler,CSK_CRYPTO_SET_RSA_RSA2048, 0);
            CRYPTO_Control(pCrypto_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_PSS);

            res = CRYPTO_RSA_Verify_Signature(pCrypto_Handler, buff, 32,
                    hdr->sign+sign_size[sign_mode]/8, crypto_rsa_e_bigendian, hdr->sign);
            break;
        }
    }while(0);

    return res;
}


#endif
