/*
 * crypto_nos_chk.c
 *
 *  Created on: 2020/10/9
 *      Author: USER
 */
#include "dbg_assert.h"
#include "Driver_CRYPTO.h"
#include "crypto_nos_chk.h"
#include "log_print.h"
#include "systick.h"
#include "ota.h"
#include "chip.h"

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t crypto_rsa_e = 0x10001;

__attribute__((aligned(32))) static uint8_t crypto_raw_data[] =
"To be, or not to be, that is the question,\n"
"Whether tis nobler in the minde to suffer\n"
"The slings and arrowes of outragious fortune,\n"
"Or to take Armes again in a sea of troubles";

static uint32_t crypto_rsa1024_n[] = {
        0xffaed2c8,0xd20bc48c,0xd815e65b,0xa3cfd69c,0x70e1b19c,0xf1ac17c8,0x6d19e410,0xff7e738e,
        0xde26f7f2,0x87da7447,0x5baae25c,0x04374ed6,0x040764ca,0xc201f6e7,0x2bd84b5c,0x96d94d49,
        0x6152bdc2,0x8d2c9399,0x0a1dfaa7,0x337b263f,0x5c0184d3,0xb2a9525a,0x8532abaf,0x0ee60bdf,
        0x775055ef,0x29508713,0x452d89ef,0x401d7c41,0xd3557cf5,0x975432f8,0x6af9a1ed,0xbbf61643
};


__attribute__((aligned(32))) static uint8_t crypto_rsa1024_p[] = {
        0x4e,0x86,0x63,0x3b,0x35,0xb5,0xaa,0xd1,0xb4,0x71,0x81,0x75,0x53,0xce,0x83,
        0x1e,0x7c,0xb4,0xe9,0x6c,0x0a,0x31,0x87,0xc4,0x33,0x50,0x9d,0xc8,0xc2,0xfc,
        0xd2,0x5a,0x0e,0x8d,0x29,0xbb,0x9c,0xc1,0x4a,0x79,0xc7,0x8e,0x32,0x02,0xfb,
        0x07,0x56,0x68,0x1f,0x23,0x81,0xc3,0x1f,0x58,0x88,0x53,0x91,0x77,0x6e,0xad,
        0x6e,0x5a,0x6c,0x8b,0xde,0xcc,0xeb,0x76,0x75,0xef,0xb3,0x3c,0x9c,0x8d,0x89,
        0xfb,0x37,0x0f,0xba,0x46,0xcb,0xb4,0xd5,0x4c,0xe1,0x72,0x21,0xac,0x01,0xad,
        0xae,0x4b,0xe3,0x7c,0x03,0x83,0x4c,0xb0,0xd4,0x5d,0x26,0xdd,0x92,0x54,0x3c,
        0xcb,0x5b,0x54,0x43,0xcb,0xf4,0x4f,0xa3,0x08,0xab,0xdd,0x23,0xb9,0x44,0x20,
        0xc9,0x68,0x4e,0x99,0x4f,0x2c,0xf2,0x71
};



static uint32_t crypto_rsa1024_nopading_destination[] = {
        0x349f8670,0x4c0c1646,0xae56a273,0xaede5cac,0xb3724516,0x22972767,0x0dc4a59f,0x588b8cab,
        0x2021a791,0x022e94e4,0xe7d5d873,0x45904234,0x06714cd3,0xf8f75545,0x56481191,0x81ef25fa,
        0xbff8882e,0xc5bfbc59,0x33e691a4,0xb76ed6f3,0xcec5bc6f,0x9373526b,0x1511f859,0x4b94f94b,
        0x683efb18,0x44ecef1b,0xf2ad5765,0xe3f579b0,0x7d1211e0,0xc1485bce,0xc1748e3c,0xb60c14eb,
};

static uint32_t crypto_rsa1024_pkcs1_destination[] =
{
        0x523ef03f,0xbd6ca3de,0xdc949a73,0x796bf00a,0x6dd3be20,0x06ecfeec,0x7e290e19,0x3311596c,
        0xc1a0dc86,0x3347f12c,0x9ca17be4,0xc9c9e36b,0x1315ca94,0x641a1aad,0x4d144415,0x1dd69704,
        0x883ed69d,0x06d35da5,0x3ced7993,0x504d348e,0x56beb2f3,0xf5373f84,0x8f7e6924,0x052fbff2,
        0x6ac37e40,0xda7f80fe,0x8627aae4,0xdebadb37,0x001a5307,0xa681d0b9,0x175d0426,0x1c824109,
};

static uint32_t crypto_rsa1024_x931_destination[] =
{
        0x7d8f56bc, 0xe8298ee8, 0x6ad0a691, 0x5e2922cf, 0xb5064279, 0x9d3654a0, 0x6800cedf, 0x03dbaf09,
        0x52309315, 0xdb0557b0, 0xd3714cb7, 0xdce10f7f, 0x80d36adf, 0x0bf9b6f6, 0xa3fdb2b3, 0xf12c4c81,
        0x45482ac1, 0x08957228, 0x07c9e59c, 0xbbb12846, 0xd15167cc, 0x3be3bba2, 0xb75edf84, 0x895c6df0,
        0x629b87b3, 0x0ca5d32c, 0xc29378cd, 0x7770d4bf, 0x7ce83cae, 0x8aa5aa25, 0x27ca6767, 0xed8b46ef,

};

void CRYPTO0_RSA1024_Encrypt_BigEndian()
{
    uint32_t crypto_rsa_e_bigendian = 0x01000100;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;
    uint32_t len;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_RSA1024, 0);
    //CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_LITTLE_ENDIAN, 1);

    systime_1 = SysTick_Value();

    // none padding
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_NONE);
    CRYPTO_RSA_Encrypt(CRYPTO0_Handler, (uint32_t*)crypto_raw_data, sizeof(crypto_raw_data)/2, encrypto_aes_result_data_32w,
            crypto_rsa1024_n, crypto_rsa_e_bigendian);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    int i = 0;
    int err_count = 0;

    for(i = 0; i < 1024/32; i++){
        if(crypto_rsa1024_nopading_destination[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, encrypto_aes_result_data_32w[i]);
        }else{
            err_count ++;
            //if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x,", i, crypto_rsa1024_nopading_destination[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA ENC with NONE padding finish!, error/total words: %d/%d, use time: %dus", err_count, 1024/32, systime_diff/CRYPTO_MAIN_FREQ);

    systime_1 = SysTick_Value();

    CRYPTO_RSA_Decrypt(CRYPTO0_Handler, crypto_rsa1024_nopading_destination, sizeof(crypto_rsa1024_nopading_destination), encrypto_aes_result_data_32w,&len,
            crypto_rsa1024_n, (uint32_t*)crypto_rsa1024_p);

    uint8_t *res = (uint8_t *)encrypto_aes_result_data_32w;
    res[len+0] = 0;
    res[len+1] = 0;
    res[len+2] = 0;
    res[len+3] = 0;
    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    err_count = 0;
    uint32_t *target_raw_data = (uint32_t *)crypto_raw_data;
    for(i = 0; i < (len+3)/4; i++){
        if(target_raw_data[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, target_raw_data[i]);
        }else{
            err_count ++;
            //if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, target_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA DEC with NONE padding finish!, error/total words: %d/%d, use time: %dus", err_count, (sizeof(crypto_raw_data)/2+3)/4, systime_diff/CRYPTO_MAIN_FREQ);

    // PKCS1 padding
    systime_1 = SysTick_Value();
    srand(systime_1);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_PKCS1);
    CRYPTO_RSA_Encrypt(CRYPTO0_Handler, (uint32_t*)crypto_raw_data, sizeof(crypto_raw_data)/2, encrypto_aes_result_data_32w,
            crypto_rsa1024_n, crypto_rsa_e_bigendian);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    i = 0;
    err_count = 0;

    CLOGD("I: RSA ENC with PKCS1 padding finish!, encrypt result, use time: %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 1024/32; i+=4){
        CLOGD("0x%08x, 0x%08x, 0x%08x, 0x%08x, ", encrypto_aes_result_data_32w[i], encrypto_aes_result_data_32w[i+1], encrypto_aes_result_data_32w[i+2], encrypto_aes_result_data_32w[i+3]);
    }

    systime_1 = SysTick_Value();

    CRYPTO_RSA_Decrypt(CRYPTO0_Handler, crypto_rsa1024_pkcs1_destination, sizeof(crypto_rsa1024_pkcs1_destination), encrypto_aes_result_data_32w,&len,
            crypto_rsa1024_n, (uint32_t*)crypto_rsa1024_p);

    res = (uint8_t *)encrypto_aes_result_data_32w;
    res[len+0] = 0;
    res[len+1] = 0;
    res[len+2] = 0;
    res[len+3] = 0;
    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    err_count = 0;
    target_raw_data = (uint32_t *)crypto_raw_data;
    for(i = 0; i < (len+3)/4; i++){
        if(target_raw_data[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, target_raw_data[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, target_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA DEC with PKCS1 padding finish!, error/total words: %d/%d, use time: %dus", err_count, (sizeof(crypto_raw_data)/2+3)/4, systime_diff/CRYPTO_MAIN_FREQ);


    // X931 padding
    len = 1024/8-20;
    systime_1 = SysTick_Value();
    srand(systime_1);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_X931);
    CRYPTO_RSA_Encrypt(CRYPTO0_Handler, (uint32_t*)crypto_raw_data, len, encrypto_aes_result_data_32w,
            crypto_rsa1024_n, crypto_rsa_e_bigendian);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    i = 0;
    err_count = 0;

    for(i = 0; i < 1024/32; i++){
        if(crypto_rsa1024_x931_destination[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, crypto_rsa1024_x931_destination[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, crypto_rsa1024_x931_destination[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA ENC with X931 padding finish!, error/total words: %d/%d, use time: %dus", err_count, 1024/32, systime_diff/CRYPTO_MAIN_FREQ);

    systime_1 = SysTick_Value();

    CRYPTO_RSA_Decrypt(CRYPTO0_Handler, crypto_rsa1024_x931_destination, 1024/8, encrypto_aes_result_data_32w,&len,
            crypto_rsa1024_n, (uint32_t*)crypto_rsa1024_p);

    res = (uint8_t *)encrypto_aes_result_data_32w;
    res[len+0] = 0;
    res[len+1] = 0;
    res[len+2] = 0;
    res[len+3] = 0;
    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    err_count = 0;
    target_raw_data = (uint32_t *)crypto_raw_data;
    for(i = 0; i < (len+3)/4; i++){
        if(target_raw_data[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, target_raw_data[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, target_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA DEC with X931 padding finish!, error/total words: %d/%d, use time: %dus", err_count, (len+3)/4, systime_diff/CRYPTO_MAIN_FREQ);


    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
}

static const uint32_t crypto_rsa2048_n[] = {
        0xcfbc44c2, 0x80cdca5b, 0x7af9ae77, 0x6f37bb34, 0xe44c765c, 0xe71d0cbb, 0xcfda0ffe, 0x7265568c,
        0xfdf92c6e, 0x4ceb4387, 0xf0d3b126, 0x6818b187, 0x2a3c7d14, 0x705dc2fa, 0x2e001119, 0x388e9cb3,
        0xebe3be08, 0x19c76e7d, 0x48597fc6, 0x27e31b84, 0xd3304630, 0x35b3fcfc, 0x1a31c475, 0x0b4cc2c0,
        0xb29501c7, 0x9b7717dc, 0xbc041509, 0x260b57db, 0x0d5459da, 0xbc89b76e, 0x8c5f9d53, 0xd29786ad,
        0x945c4f48, 0xcf2f30dd, 0x3120defc, 0x25299d25, 0x5bd2b778, 0x085b995d, 0x89798112, 0x408fcfa0,
        0x3b7277b1, 0x4355fc13, 0x41d52970, 0x2d4b31ed, 0x99cf7d6c, 0x9f72d15f, 0xde96328b, 0x77198b5d,
        0xbf09ff75, 0x3dd7e926, 0xcf811ac7, 0xbf891b05, 0x5ebf3245, 0x335ce3c9, 0xf447724a, 0x389bae24,
        0xa29a7624, 0x4950509a, 0x55b926f5, 0x14c947a6, 0xa8d4caa2, 0x5ae99f8a, 0x30aa125a, 0x398b78d5,
};


static const uint32_t crypto_rsa2048_p[] = {
        0x8eb95d22, 0xbd911cef, 0xe81aaf03, 0x8b0bf300, 0x4de52df2, 0xfc713f63, 0x3c4fc7eb, 0x9d7b057f,
        0xc0c71ac2, 0x0bb7508f, 0x30a41eba, 0x6a1938fd, 0x773111b4, 0x4606f422, 0x99add081, 0x10016215,
        0x4f638fad, 0x748ad971, 0xebb85627, 0x4fac9f28, 0xcfc3ecee, 0x87098684, 0x70fc04d0, 0x87ae9fd0,
        0x6fb1d538, 0xa8161b3a, 0x6accf300, 0x16045d42, 0x79e0f283, 0x0f6fd81d, 0x45f434b7, 0xb5c51eb5,
        0xa3d3a778, 0x7bbc3523, 0xee7d5901, 0x28da4fb9, 0xab255dad, 0x61b06a66, 0xeea712f6, 0x8bb1e7d1,
        0xb5ba2991, 0x6bc878f8, 0xe8326776, 0xba594ef3, 0xecc044c1, 0xb2637c8d, 0x33b90c6e, 0xae8d0c42,
        0x8ac8544e, 0xc847f9ef, 0x46c88499, 0x5953a6f6, 0xd7e360f8, 0xf595101d, 0x18a3f46d, 0x0414d740,
        0xd6698cac, 0xccd8dc14, 0xd7ac1cbc, 0x297e2b21, 0xf4a00688, 0x04140806, 0x8433324d, 0xcf8e209c
};



static const uint32_t crypto_rsa2048_nopading_destination[] = {
        0x1d7f964c,        0x70cd3a49,        0x74fab889,        0x6b0c6308,
        0xfc1ce5d6,        0x6902de84,        0xf37df521,        0xe170723f,
        0xd2107dd3,        0x611affaf,         0x96e1a303,         0x98c5e1c8,
         0x85db673f,         0xde85f777,         0xcb490545,         0x3b756f96,
         0x825f36be,         0xdf8bc08c,         0xeb7a7c27,         0x57cd6da0,
         0x39031a3a,         0x910f4128,         0x56eb0d47,         0x67f31374,
         0x288d2007,         0x67404f18,         0x6825ff3a,         0x7be85c60,
         0x1ec40f02,         0x42582964,         0x16d5ab4e,         0xf78921f6,
         0x661408a2,         0x8881b228,         0x6f8afceb,         0xa0a56bc2,
         0x19300055,         0x5c8246bb,         0x78a3e26b,         0xa8f5c0c7,
         0x99556072,         0x0da2cd77,         0x7a0357c7,         0x05060087,
         0xb6ce15b6,         0x7f4cd0b9,         0x6be0a5af,         0xebdbd09d,
         0x2df6ca25,         0x90194e13,         0x69deb4f6,         0xabfbed71,
         0x9efd2854,         0x4bbea156,         0x3c460740,         0x76061b33,
         0xd03692bd,         0xffd51abc,         0xb0a077cf,         0xaa29bff2,
         0x8925bd4f,         0x5360546f,         0xb2bbcf11,         0x6e567953,
};

static const uint32_t crypto_rsa2048_pkcs1_destination[] =
{
    0x6fea5d58,0x50d75197,0xfa14854f,0x504e6652,0xfd95242c,0xaea40934,0xab507394,0x4e96891f,
    0xa7023a9e,0x4750bb96,0x96db4f60,0xac9d99b7,0x091d2189,0xda7b0622,0xbdaf3557,0x92802555,
    0x8a4fb83a,0x7b4ce6b4,0x4d9ac1c9,0xaef91a7c,0x009413cb,0xcddce306,0xe0e87704,0xdc810c13,
    0x5113efbb,0xce7523ce,0x3790f704,0x989cf964,0x993bdb1d,0x316cc13e,0x08e05423,0x6b90502c,
    0x415bc16a,0x6909602b,0x5d7e131f,0x7fcaebab,0x20e44e64,0xdd509386,0x709d0ff3,0x2a0574bd,
    0x89c2a7a5,0x9bfa203b,0xad87321f,0x716d866c,0xcbd634e9,0x181d5cca,0xa4d21c15,0xc6dd3b00,
    0x17c2ecc8,0x1defc031,0x3ddee638,0x8b719c59,0xb22fa946,0x7ab210ef,0xc224b62c,0x36865c08,
    0x6be14345,0xc409ff67,0x32b3d044,0x123a54c4,0x89645c48,0xff5e9cfa,0x81c272d8,0x7ccb8182,
};

static const uint32_t crypto_rsa2048_oaep_destination[] =
{
    0xaad02e92, 0x64f38b75, 0x5f03dc0b, 0xd4dea3b0, 0x7a5d29c4, 0x5c9d4469, 0x555d184f, 0xbe215a58,
    0x15f03c3e, 0x961c37f4, 0x1f99facf, 0x7c005c74, 0x721fa258, 0x4723cd9b, 0x2a775ce8, 0x66835bbe,
    0x9deaf972, 0xb0104f6d, 0x8c62c65a, 0x4abad972, 0x3cbc9d10, 0x4ec8e886, 0x1adba457, 0xd5641c80,
    0xf1e7fe28, 0x0219d849, 0x455a43a3, 0xba28b7c4, 0xc50e1a50, 0x3239c3eb, 0x2c2ff465, 0xdd3e619c,
    0xbb23c72d, 0xab8a81dc, 0xd3561405, 0x5f0581c5, 0xe0386cd6, 0xdf50bf90, 0x2cb9bf09, 0x268752aa,
    0x450ca461, 0x16a41b9f, 0x85346a8d, 0x186c28eb, 0x0a35e1e2, 0x96de1e56, 0xe0862dc0, 0xd64d8189,
    0x3a61a8e5, 0x57f6f0c9, 0x25b94c3f, 0xe713934e, 0xb7071208, 0x6e55bcc9, 0xe9d0a394, 0xc0b10f48,
    0xf7b74710, 0x93ae1c64, 0x1cecdefc, 0xcd9aa9df, 0xd129f428, 0x3cff8453, 0x63267303, 0x4f1506a8,
};

void CRYPTO0_RSA2048_Encrypt_BigEndian()
{
    uint32_t crypto_rsa_e_bigendian = 0x01000100;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;
    uint32_t len;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_RSA2048, 0);
    //CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_LITTLE_ENDIAN, 1);

    systime_1 = SysTick_Value();

    // none padding
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_NONE);
    CRYPTO_RSA_Encrypt(CRYPTO0_Handler, (uint32_t*)crypto_raw_data, sizeof(crypto_raw_data)-1, encrypto_aes_result_data_32w,
            crypto_rsa2048_n, crypto_rsa_e_bigendian);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    int i = 0;
    int err_count = 0;

    for(i = 0; i < 2048/32; i++){
        if(crypto_rsa2048_nopading_destination[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, encrypto_aes_result_data_32w[i]);
        }else{
            err_count ++;
            //if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x,", i, crypto_rsa2048_nopading_destination[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA ENC with NONE padding finish!, error/total words: %d/%d, use time: %dus", err_count, 2048/32, systime_diff/CRYPTO_MAIN_FREQ);

    systime_1 = SysTick_Value();

    CRYPTO_RSA_Decrypt(CRYPTO0_Handler, crypto_rsa2048_nopading_destination, sizeof(crypto_rsa2048_nopading_destination), encrypto_aes_result_data_32w,&len,
            crypto_rsa2048_n, crypto_rsa2048_p);

    uint8_t *res = (uint8_t *)encrypto_aes_result_data_32w;
    res[len+0] = 0;
    res[len+1] = 0;
    res[len+2] = 0;
    res[len+3] = 0;
    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    err_count = 0;
    uint32_t *target_raw_data = (uint32_t *)crypto_raw_data;
    for(i = 0; i < (len+3)/4; i++){
        if(target_raw_data[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, target_raw_data[i]);
        }else{
            err_count ++;
            //if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, target_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA DEC with NONE padding finish!, error/total words: %d/%d, use time: %dus", err_count, (sizeof(crypto_raw_data)+3)/4, systime_diff/CRYPTO_MAIN_FREQ);

    // PKCS1 padding
    systime_1 = SysTick_Value();
    srand(systime_1);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_PKCS1);
    CRYPTO_RSA_Encrypt(CRYPTO0_Handler, (uint32_t*)crypto_raw_data, sizeof(crypto_raw_data)-1, encrypto_aes_result_data_32w,
            crypto_rsa2048_n, crypto_rsa_e_bigendian);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    i = 0;
    err_count = 0;

    CLOGD("I: RSA ENC with PKCS1 padding finish!, encrypt result, use time: %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 2048/32; i+=4){
        CLOGD("0x%08x, 0x%08x, 0x%08x, 0x%08x, ", encrypto_aes_result_data_32w[i], encrypto_aes_result_data_32w[i+1], encrypto_aes_result_data_32w[i+2], encrypto_aes_result_data_32w[i+3]);
    }

    systime_1 = SysTick_Value();

    CRYPTO_RSA_Decrypt(CRYPTO0_Handler, crypto_rsa2048_pkcs1_destination, sizeof(crypto_rsa2048_pkcs1_destination), encrypto_aes_result_data_32w,&len,
            crypto_rsa2048_n, crypto_rsa2048_p);

    res = (uint8_t *)encrypto_aes_result_data_32w;
    res[len+0] = 0;
    res[len+1] = 0;
    res[len+2] = 0;
    res[len+3] = 0;
    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    err_count = 0;
    target_raw_data = (uint32_t *)crypto_raw_data;
    for(i = 0; i < (len+3)/4; i++){
        if(target_raw_data[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, target_raw_data[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, target_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA DEC with PKCS1 padding finish!, error/total words: %d/%d, use time: %dus", err_count, (sizeof(crypto_raw_data)+3)/4, systime_diff/CRYPTO_MAIN_FREQ);

    // OAEP padding
    systime_1 = SysTick_Value();
    srand(systime_1);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_OAEP);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_LABEL, (uint32_t)"label");
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA256);

    CRYPTO_RSA_Encrypt(CRYPTO0_Handler, (uint32_t*)crypto_raw_data, sizeof(crypto_raw_data)-1, encrypto_aes_result_data_32w,
            crypto_rsa2048_n, crypto_rsa_e_bigendian);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    i = 0;
    err_count = 0;

    CLOGD("I: RSA ENC with OAEP padding finish!, encrypt result, use time: %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 2048/32; i+=4){
        CLOGD("0x%08x, 0x%08x, 0x%08x, 0x%08x, ", encrypto_aes_result_data_32w[i], encrypto_aes_result_data_32w[i+1], encrypto_aes_result_data_32w[i+2], encrypto_aes_result_data_32w[i+3]);
    }

    systime_1 = SysTick_Value();

    CRYPTO_RSA_Decrypt(CRYPTO0_Handler, crypto_rsa2048_oaep_destination, sizeof(crypto_rsa2048_oaep_destination), encrypto_aes_result_data_32w,&len,
            crypto_rsa2048_n, crypto_rsa2048_p);

    res = (uint8_t *)encrypto_aes_result_data_32w;
    res[len+0] = 0;
    res[len+1] = 0;
    res[len+2] = 0;
    res[len+3] = 0;
    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    err_count = 0;
    target_raw_data = (uint32_t *)crypto_raw_data;
    for(i = 0; i < (len+3)/4; i++){
        if(target_raw_data[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, target_raw_data[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, target_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA DEC with OAEP padding finish!, error/total words: %d/%d, use time: %dus", err_count, (sizeof(crypto_raw_data)+3)/4, systime_diff/CRYPTO_MAIN_FREQ);


    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
}

static uint32_t crypto_rsa4096_n[] = {
        0xea6afaa4, 0x2c03514e, 0xfc16f103, 0x2773641c, 0xf2f6e893,
        0x5720c5e4, 0x0b9173b2, 0xa4af3c99, 0xc46afecd, 0x0f5c54f3, 0x4708cb5d, 0xd65d9eba, 0x823bf382,
        0x1313e146, 0x277d770b, 0x3b3b7a46, 0x38d20f8f, 0x2baec19c, 0x6b7a69ed, 0x4da68282, 0xe6961586,
        0x9fbfac4e, 0x6473902f, 0x270ecdbe, 0xc5a14e3b, 0x08cb7a38, 0x7ed18585, 0xcbb6d788, 0x5b462bb7,
        0x8002b3e4, 0x14e17d53, 0x20a0c74e, 0x7101d3cf, 0x622b7ba1, 0xca78e5f8, 0xda3a7b20, 0x5e4dc47b,
        0x8857ad9f, 0x341592a7, 0x735bd6b3, 0xedbae59e, 0x90dd95e1, 0x3c00fea8, 0xe53bdf50, 0xce23761c,
        0xf5436c28, 0xdbd4fce7, 0x9bc950d1, 0x74879998, 0xfd013289, 0xa7385004, 0xef04ce72, 0xa2f040de,
        0xd3a7aed8, 0x0fb7b05f, 0x4dc756cc, 0x628b2550, 0x19c94eab, 0x36888d40, 0x4b622c68, 0x78de9a36,
        0xf438c920, 0xb634f4ca, 0xbbc89c73, 0xe6b6edb8, 0x5a35bafe, 0x6615f780, 0xf4d4b174, 0xd949bdf0,
        0x7a5132b6, 0xbd3b26bf, 0xd26076cb, 0xded3d2db, 0x5ab7f5f9, 0xfc9fc963, 0x422c9888, 0xe99690da,
        0x7aa0b05a, 0xb8e9cc6f, 0x51c1a672, 0xa8580249, 0xdd91458c, 0x7201e1fa, 0x30674ef8, 0x284e76d8,
        0xabc246fb, 0x33a17089, 0xac89f69d, 0xa96fb649, 0x73b5890c, 0xa9abc6c5, 0xeb656839, 0xf459d21b,
        0x363f3adb, 0x80ab35e8, 0x9cde54ce, 0x60111b6e, 0xc1aa9268, 0x9b5cfd44, 0xc2b63af0, 0x9432beed,
        0xc40a81e5, 0x7e71e4d6, 0x62629bb3, 0x68e66a2f, 0x59a19f37, 0x60471b5e, 0x2462cc1d, 0x00475d83,
        0xdb13e98d, 0x88772a6b, 0xba999811, 0x5625def9, 0x5d1ebcdd, 0xcc167b88, 0x0d76ddb8, 0x391bd0cd,
        0x7b01bd63, 0x01ad00d4, 0x78e02d11, 0xf99e5afd, 0x155165da, 0x2b042cbd, 0x8f2b9f04, 0xb887c7db,
        0xc5e8563d, 0x23f34753, 0x8f497ea7,
};


static uint32_t crypto_rsa4096_p[] = {
        0xdcbe7401, 0x6f10a035, 0xc048d265, 0xb0db3efe, 0xefff6b7d, 0xddea4b52, 0x7e0e2369, 0xc6532317,
        0xaa6b05c5, 0x567b4dd9, 0x0accf8a6, 0xe1360a38, 0xff8f8cf2, 0xe05cdaef, 0x18c51560, 0x729a15a4,
        0xb2663666, 0x939b40d6, 0xa645fd32, 0x76f1a85e, 0x895be47a, 0x4be0a7bf, 0x95774158, 0x003284ec,
        0xa973d2e0, 0x2c07a488, 0xf3f27660, 0x5fbb3647, 0xff84e7f2, 0x0c20438c, 0x8e6884ca, 0x2513ec20,
        0x7df41b46, 0x4e6e320d, 0x66e5f9dc, 0xaac63662, 0x27333ecf, 0x91e1d181, 0xe12f6172, 0x2dae262b,
        0x182e68fc, 0x6b52b7ea, 0xb51750db, 0xf7416cd5, 0x190716eb, 0x9fb2ef89, 0xe77edff7, 0x54dd9683,
        0x98634535, 0x657776a2, 0xe3a13a96, 0x953e1c8e, 0x9d6a6273, 0x8a1d6f30, 0x8395e1df, 0xa67d45d5,
        0x62e360e2, 0x4d0c6ebe, 0x801e6b3d, 0xff4d3013, 0x172a25da, 0x175fc073, 0x1be55395, 0xe70c18d2,
        0x2a632ff1, 0xb0d7dd08, 0xf7ab5f86, 0x8b5186df, 0x884b3456, 0xfa935eef, 0x5e17b0be, 0x9a500540,
        0x028a6e44, 0xae70009a, 0x526293e6, 0x324861b6, 0x226cc928, 0xafb5a53b, 0x50ebc39c, 0x2e245e5b,
        0xf034a724, 0x3c83b604, 0x338a9896, 0xfd76f8a1, 0x247c9041, 0x5ba5dbdb, 0x71c54b28, 0x4f210139,
        0xf2c25c42, 0xffa02d39, 0x9b8c0527, 0xcbae369a, 0xb6da54df, 0x2b11a42f, 0x11e83990, 0x2fc01106,
        0x74b55bb3, 0xb5632199, 0x0ccaebff, 0x7e29f9c1, 0x312863a0, 0x501d289d, 0x75c4941a, 0xfdc4106b,
        0xbe8428f7, 0x7843ada6, 0x71e7ec46, 0xfadae0e6, 0xdca3ce9e, 0x94016c46, 0x9074a6af, 0x2317e025,
        0xed739a08, 0xc25bf977, 0xedfc4e94, 0x8e0bfbb0, 0xdf6e41a0, 0x4d78516a, 0x32b0f3d6, 0xe6ec9259,
        0xac6b957e, 0xa648c4df, 0x775bce6d, 0x8d2910f0, 0xcfdddb0b, 0xe106df9a, 0x3e439a09, 0x02bd2589,
};



static const uint32_t crypto_rsa4096_nopading_destination[] = {
 0x1ef6b043, 0x049e7e76, 0xd1c46f93, 0xeb420b2c, 0x5792c981, 0xcc149163, 0xcf9c69a4, 0xe59da9d7,
 0xfddd3194, 0xb05d8b85, 0x1494d182, 0x3074067b, 0x83394056, 0x7fd6b9db, 0x3f436a0e, 0x1b8cee3a,
 0x36278fa3, 0xd4e4aa71, 0x87052f69, 0x4f12d9e6, 0x0a827781, 0x63685303, 0x138b5926, 0x3ae53af8,
 0x7d69d81b, 0x0abac329, 0x4e2a4005, 0xeaa44547, 0x9ea335b0, 0x429145de, 0x851b0a7e, 0xb7541e0c,
 0x240d56f3, 0x35fb41e0, 0x08f52e06, 0x7ac63eab, 0x57c300f7, 0xe9133314, 0xea10e62d, 0x6171eb13,
 0x8b7abe97, 0x8432f3b1, 0x0c87693d, 0x75cb1626, 0x5d331e55, 0x9ca89251, 0xa39280af, 0x00401771,
 0x6aa52bf0, 0x2aa971c1, 0x81ed5a76, 0x64266899, 0x3f7b2bde, 0x0ecdcd65, 0xceea52f1, 0x2fb7aba5,
 0x8eff01fd, 0x786d56d3, 0x5b6ee49d, 0x985e96eb, 0x0531aff3, 0x70cf3d91, 0xff16c8c7, 0x144c5847,
 0x49f70847, 0x5872d546, 0x1095c789, 0x666db0c2, 0x67337719, 0x672eb878, 0xfa238a69, 0x2802c202,
 0xcd06731f, 0xdf58a69f, 0x5aa1acca, 0x683ec902, 0xa761976f, 0x0f0b36fa, 0x8508c682, 0xa47588c6,
 0xddd46895, 0xf8569e77, 0x0b6e848f, 0xc6445ea4, 0xb3a33f90, 0x9c2d40d3, 0x05121300, 0x9b8e101e,
 0x1092102f, 0x7ce613d0, 0x1277dd26, 0xf415e122, 0xd91c56ae, 0x1b58869e, 0x2209dff2, 0x0c5919bd,
 0xc49e8f5c, 0x5e6a8f09, 0x7b839336, 0x678c3e2c, 0x3c7e4cda, 0xf64b721c, 0xa9c7c78c, 0x14c5e4bd,
 0x6d433d96, 0x27975790, 0x3d267641, 0x53cb2740, 0x0120e222, 0xc92013f8, 0x84bd0b22, 0xfe468f9b,
 0x8ecf22d6, 0x6179db9d, 0x01842767, 0xce630395, 0xf8c938d1, 0xb1dec451, 0xcb2283fc, 0xd40df1a2,
 0x95ea66e0, 0x2adc2e3c, 0x1d5ca89c, 0xe482b3b1, 0x4c42bf15, 0x28939dc7, 0x549a6f65, 0xc720114b,
};

static const uint32_t crypto_rsa4096_pkcs1_destination[] =
{
        0x3a588541, 0xc8369b2d, 0xee240217, 0x16a0dbf3,
        0xa93bf33f, 0xb687f907, 0xfaa92c38, 0x2de6ae5c,
        0x1fa65c71, 0x8080b9aa, 0x775b4bf8, 0x242e6969,
        0xa42739aa, 0x23d9cc16, 0xf9f0581c, 0x1868b92e,
        0x754f4e20, 0x02f6fcf7, 0xc88b249d, 0x88c0bd21,
        0xf52dd18a, 0x13701c2a, 0x4a117ed1, 0x13b88f4b,
        0xbbf53c92, 0x4968ddd5, 0x5abcfb1e, 0x15795869,
        0xad768535, 0x8b58a690, 0x1189043f, 0xb3398b5c,
        0x56f23df7, 0x33858000, 0xf2db032a, 0x175b0356,
        0x8573dd5c, 0xbfc51be1, 0x65d42cfe, 0x53841a39,
        0x064f87d6, 0x01d9b9ad, 0x138d6d6c, 0x43a3d971,
        0x7fd8ac77, 0x33719077, 0xc2a49f91, 0x9a3087a2,
        0x05336253, 0x421f6d31, 0xee75cac2, 0x1d586f4f,
        0x08895da9, 0xf95e324a, 0x2174e610, 0x3836f873,
        0xbbfb650c, 0x58e6a018, 0xbd0e5103, 0x03802bed,
        0xfa5e14af, 0x27459122, 0xfa27344d, 0xa6ff8d68,
        0xcd2b0ce5, 0xf000c968, 0x63ac20d4, 0xd7d985ee,
        0x48e9b2ed, 0xacaf2d8a, 0x4a1ead9c, 0x331c3a55,
        0xf74f4251, 0x66ef8f5d, 0xc19c0095, 0x9d0c57fa,
        0x54a2bef2, 0x46e00ef0, 0x0206b05a, 0x99ca94e4,
        0xe39b6321, 0x5cf5a54d, 0xfc9740bf, 0x7254bf09,
        0xd6fbac52, 0x12f8092e, 0x2dac3143, 0xaaf1d530,
        0x081968c7, 0x1dfb3891, 0xcd964815, 0x0c7b0332,
        0xa300e734, 0x52c6c429, 0x189f5009, 0x05fcf26a,
        0xdfc1db7f, 0xbf5ba306, 0xdd884c10, 0x92fee971,
        0x7abcb81d, 0x5c8a01c1, 0x603121bb, 0xd7ab3f5b,
        0x75ad1760, 0xdcfd6e86, 0xccc75641, 0x7ac4e620,
        0x9826da8f, 0x4537781c, 0x2a06ad2a, 0xe1ced007,
        0xfb2b5ba8, 0xff8295f3, 0xb45b1640, 0x0269a0f4,
        0x5f31dfe9, 0xb4122f42, 0x828ab450, 0x58fa67fc,
        0x3a6d08bf, 0x2a1659fd, 0x1e60a7db, 0xf5d83208,
        0x032c5b25, 0xba303ef7, 0x373d1110, 0x43136c6e,
};

static const uint32_t crypto_rsa4096_oaep_destination[] =
{
        0xc2000e5d, 0xdc56d934, 0xd4360fe8, 0x594d5bde, 0xac8f69c6, 0x35e7ed80, 0x430961bd, 0x58fd109c,
        0x80906127, 0x0302a67c, 0x40dda2cf, 0x30251ee8, 0xc9d88d87, 0x2b235138, 0x80049fef, 0xe0e73bb3,
        0x6a4dafd4, 0x705c0017, 0xa1d15f5b, 0x8b4d21c8, 0xc6bb4797, 0x38ff6c10, 0x262ab80c, 0xbb434a45,
        0x94378e2f, 0xf6759d02, 0x92b6f333, 0x8c901175, 0xc2822bf1, 0xc4b3cdd3, 0x92cc6695, 0xf922aa6a,
        0xd9c71633, 0x20455827, 0x9a7866f8, 0x3f7975d8, 0x84a7caa9, 0x9faee607, 0xdd6059f4, 0x39a7423f,
        0x96f717d4, 0x283ac186, 0x8ffd2bcd, 0x239dc2ff, 0x5c7d704d, 0x3ba1d91c, 0x6d3711b0, 0x70327897,
        0xf248ad47, 0x69943430, 0x2507d221, 0x15b5ffd2, 0x2d6a3f54, 0x6cb8b149, 0xff71e181, 0x50b076ab,
        0xd12613a4, 0x4d8e6e60, 0x45e71832, 0xf09519d5, 0x71f7d8ee, 0x83df56f5, 0xdf86cc38, 0x6ddceac0,
        0xb819f47a, 0xe36ebae8, 0xbbefeffe, 0xcdde9a89, 0x97ae09e2, 0x22036738, 0x3a41226c, 0x5c0012a3,
        0x341bc2ef, 0xdd1da817, 0x4d19da14, 0xb33aa554, 0x5794dd78, 0x25352ee6, 0xe2c11ded, 0x635974f5,
        0x0f12e111, 0x12cddc92, 0x485b0379, 0x3a2e76c9, 0xa9fc01aa, 0xa1cd375a, 0x7e521161, 0x86119d1b,
        0x98f10577, 0x1510719c, 0x0d5487f8, 0x100b0815, 0xab94b6cd, 0xd924dd52, 0x1765453e, 0xa759c030,
        0xab8552fe, 0x8229645f, 0x5a9fd007, 0x86d3e361, 0xde0e31dc, 0x46af9c0a, 0x58e25a78, 0x035e8648,
        0x35feba12, 0xe6b412bc, 0xac8748ed, 0xb4c05234, 0x2719bab3, 0x4f0e2399, 0xccb1db1d, 0x41a5c04b,
        0xcb3ce33b, 0x0f5427e6, 0x79c841e0, 0x2bacd63f, 0x6b498d0c, 0x0ed49e1f, 0x0278b50b, 0xa8f4045b,
        0x5f48dce9, 0x05d708d7, 0xc6d4d81b, 0x9cd7362c, 0xda1f1060, 0x1586e3c1, 0x7bf67922, 0x4b1287f0,
};


void CRYPTO0_RSA4096_Encrypt_BigEndian()
{
    uint32_t crypto_rsa_e_bigendian = 0x01000100;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;
    uint32_t len;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_RSA4096, 0);
    if(crypto_rsa4096_p[0] == 0xdcbe7401)
        CRYPTO_SWAP_Bytes(CRYPTO0_Handler, crypto_rsa4096_p, sizeof(crypto_rsa4096_p), crypto_rsa4096_p);
    //CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_LITTLE_ENDIAN, 1);

    systime_1 = SysTick_Value();

    // none padding
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_NONE);
    CRYPTO_RSA_Encrypt(CRYPTO0_Handler, (uint32_t*)crypto_raw_data, sizeof(crypto_raw_data)-1, encrypto_aes_result_data_32w,
            crypto_rsa4096_n, crypto_rsa_e_bigendian);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    int i = 0;
    int err_count = 0;

    for(i = 0; i < 4096/32; i++){
        if(crypto_rsa4096_nopading_destination[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, encrypto_aes_result_data_32w[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x,", i, crypto_rsa4096_nopading_destination[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA ENC with NONE padding finish!, error/total words: %d/%d, use time: %dus", err_count, 4096/32, systime_diff/CRYPTO_MAIN_FREQ);

    systime_1 = SysTick_Value();

    CRYPTO_RSA_Decrypt(CRYPTO0_Handler, crypto_rsa4096_nopading_destination, sizeof(crypto_rsa4096_nopading_destination), encrypto_aes_result_data_32w,&len,
            crypto_rsa4096_n, crypto_rsa4096_p);

    uint8_t *res = (uint8_t *)encrypto_aes_result_data_32w;
    res[len+0] = 0;
    res[len+1] = 0;
    res[len+2] = 0;
    res[len+3] = 0;
    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    err_count = 0;
    uint32_t *target_raw_data = (uint32_t *)crypto_raw_data;
    for(i = 0; i < (len+3)/4; i++){
        if(target_raw_data[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, target_raw_data[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, target_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA DEC with NONE padding finish!, error/total words: %d/%d, use time: %dus", err_count, (sizeof(crypto_raw_data)+3)/4, systime_diff/CRYPTO_MAIN_FREQ);

    // PKCS1 padding
    systime_1 = SysTick_Value();
    srand(systime_1);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_PKCS1);
    CRYPTO_RSA_Encrypt(CRYPTO0_Handler, (uint32_t*)crypto_raw_data, sizeof(crypto_raw_data)-1, encrypto_aes_result_data_32w,
            crypto_rsa4096_n, crypto_rsa_e_bigendian);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    i = 0;
    err_count = 0;

    CLOGD("I: RSA ENC with PKCS1 padding finish!, encrypt result, use time: %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 4096/32; i+=4){
        CLOGD("0x%08x, 0x%08x, 0x%08x, 0x%08x, ", encrypto_aes_result_data_32w[i], encrypto_aes_result_data_32w[i+1], encrypto_aes_result_data_32w[i+2], encrypto_aes_result_data_32w[i+3]);
    }

    systime_1 = SysTick_Value();

    CRYPTO_RSA_Decrypt(CRYPTO0_Handler, crypto_rsa4096_pkcs1_destination, sizeof(crypto_rsa4096_pkcs1_destination), encrypto_aes_result_data_32w,&len,
            crypto_rsa4096_n, crypto_rsa4096_p);

    res = (uint8_t *)encrypto_aes_result_data_32w;
    res[len+0] = 0;
    res[len+1] = 0;
    res[len+2] = 0;
    res[len+3] = 0;
    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    err_count = 0;
    target_raw_data = (uint32_t *)crypto_raw_data;
    for(i = 0; i < (len+3)/4; i++){
        if(target_raw_data[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, target_raw_data[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, target_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA DEC with PKCS1 padding finish!, error/total words: %d/%d, use time: %dus", err_count, (sizeof(crypto_raw_data)+3)/4, systime_diff/CRYPTO_MAIN_FREQ);

    // OAEP padding
    systime_1 = SysTick_Value();
    srand(systime_1);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_OAEP);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_LABEL, (uint32_t)"label");
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA1);

    CRYPTO_RSA_Encrypt(CRYPTO0_Handler, (uint32_t*)crypto_raw_data, sizeof(crypto_raw_data)-1, encrypto_aes_result_data_32w,
            crypto_rsa4096_n, crypto_rsa_e_bigendian);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    i = 0;
    err_count = 0;

    CLOGD("I: RSA ENC with OAEP padding finish!, encrypt result, use time: %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 4096/32; i+=4){
        CLOGD("0x%08x, 0x%08x, 0x%08x, 0x%08x, ", encrypto_aes_result_data_32w[i], encrypto_aes_result_data_32w[i+1], encrypto_aes_result_data_32w[i+2], encrypto_aes_result_data_32w[i+3]);
    }

    systime_1 = SysTick_Value();

    CRYPTO_RSA_Decrypt(CRYPTO0_Handler, crypto_rsa4096_oaep_destination, sizeof(crypto_rsa4096_oaep_destination), encrypto_aes_result_data_32w,&len,
            crypto_rsa4096_n, crypto_rsa4096_p);

    res = (uint8_t *)encrypto_aes_result_data_32w;
    res[len+0] = 0;
    res[len+1] = 0;
    res[len+2] = 0;
    res[len+3] = 0;
    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    err_count = 0;
    target_raw_data = (uint32_t *)crypto_raw_data;
    for(i = 0; i < (len+3)/4; i++){
        if(target_raw_data[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, target_raw_data[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, target_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: RSA DEC with OAEP padding finish!, error/total words: %d/%d, use time: %dus", err_count, (sizeof(crypto_raw_data)+3)/4, systime_diff/CRYPTO_MAIN_FREQ);


    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
}

static uint32_t rsa_sig_digest[] = {
        0xd6d450c8,0xa366b6b9,0x761a10e7,0xa1596834,0xbbcaa23b,0x8af2d430,0x123a0766,0xa572ef71
};

static const uint32_t crypto_rsa2048_pkcs1_signature[] =
{
        0xd774ed9d,0xe6294373,0x15e3a074,0x70d38550,0x5c75f6ef,0x0c167cd3,0xcddae349,0x0042f1f3,
        0xcf9be2cd,0x86c5892e,0x1e9278e2,0x89ba865c,0x210683df,0x0ccedca7,0x4d25e00d,0xf8cd3c46,
        0x4e8dc8d1,0xf923b5c1,0x01069afc,0xde7a30d1,0xf39a2e22,0x0d5f793c,0x16f7d56f,0xfd5758a5,
        0xc7db4db6,0x5423b84b,0x267248c3,0xd40a61d7,0xe0d0ed87,0xf8eb2554,0x2c0663f6,0x773accc5,
        0x70d76722,0xaadfa56f,0x2fb8004e,0x054a4e65,0x5d19b56c,0x93dd69b8,0x10df0510,0x8185c5f7,
        0xf39dcf17,0x911e6a08,0xe19a7ca4,0xf8f063d1,0x51ece002,0x02d0ed71,0x1e4dfcc3,0x6283d520,
        0x72442d7c,0xfa3d532d,0xc7ea26f2,0x961f2e63,0x3415e086,0x4f49cfee,0xe03108fa,0xac8d946e,
        0x605dd282,0xb2f5d05b,0xe1ab8b76,0xb5e36188,0x855a818d,0x133c5344,0x4626c02c,0xf01fec61,
};

static const uint32_t crypto_rsa2048_x931_signature[] =
{
        0x0df78dbc, 0xeae89826, 0xa55a5c52, 0xb38fd06a,0x19f067ec, 0xb2ce006d, 0xcc05df2f, 0x3a18451d,
        0xd5dee0e8, 0xff4e1e62, 0xe1169d5e, 0x78f08c31,0x7a0bf7a5, 0x522162e3, 0x9dc6f46a, 0xf1438704,
        0x0342c132, 0x40f764ca, 0x9517bca1, 0x6d9435ae,0x064e2321, 0x358176ce, 0x3d467735, 0x94f92bd8,
        0x4766ddcf, 0x78e5f968, 0x97b8f2a8, 0xde315132,0xfce38127, 0x97413016, 0xa8e8a5f2, 0x155a0149,
        0x5bd64997, 0x0224ffed, 0x0209b187, 0xfb5bcc50,0xec53e970, 0x0788d6d4, 0x2affdf15, 0xebe50587,
        0x1c9daf50, 0xad51bb83, 0xbfe616bd, 0xda05b957,0xab93baff, 0x08463b99, 0x0f6aef8a, 0x0341724c,
        0xd23a93e8, 0xd7c624ea, 0xc16bdb13, 0x2d886525,0xe544e924, 0xb3213495, 0xff908ad2, 0x2e0a1ec4,
        0x9e7a903c, 0x04b17f19, 0xb04a24a1, 0xb2925007,0x7c1b88a0, 0x9f5a57ef, 0x7d3892e6, 0x8fb690da,

};

static const uint32_t crypto_rsa2048_pss_signature[] =
{
    0x37fc6d48, 0x7e8a6bd9, 0x0e920eb6, 0x9eb57610, 0xd3356f81, 0x5fd15311, 0xcacf23ee, 0x3d8ffe0b,
    0x6c01df02, 0x26b310a4, 0x35a7b77f, 0xa624dd5b, 0x065211bf, 0x26dc9c8e, 0x4b4bb069, 0xae9ef9b8,
    0x6be8ace6, 0xa2285a60, 0x1280fdc3, 0x0f899855, 0xd14b0bf4, 0xacdd491c, 0xf27b1c66, 0x28c6adb5,
    0x92378efc, 0xdf3eab86, 0xb919c528, 0x09adbafe, 0xe12dcdd9, 0x08fa23c9, 0xbbb6fd91, 0x374e84ce,
    0x5be9cb51, 0x7b160fbb, 0x93b6b9c2, 0x47f64d0d, 0x58b21794, 0x48ef155e, 0xac442e28, 0x0051fb8c,
    0x5d056c8f, 0xd44494ef, 0xb29a4df4, 0xf5bd7842, 0x1d9852e5, 0x5f128412, 0x3bb08f21, 0x1f10d81a,
    0x45c956f3, 0x3383e150, 0x82ce059a, 0x222d6f9d, 0x94431fa0, 0x06fbe3aa, 0x25690624, 0x1e4eb848,
    0x5f1221f1, 0x493e17c9, 0x1626688d, 0x85f9cc34, 0x1ee85180, 0xf10557c0, 0x9ae67c99, 0x7b05375d,
};

void CRYPTO0_RSA2048_Signature_BigEndian()
{
    uint32_t crypto_rsa_e_bigendian = 0x01000100;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;
    uint32_t len;
    int i;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA256);

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)crypto_raw_data, sizeof(crypto_raw_data)-1, encrypto_aes_result_data_32w, 0);

    CLOGD("HASH256 finished, digest value check:");
    for(i = 0; i < 8; i++){
        if(encrypto_aes_result_data_32w[i] == rsa_sig_digest[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, target data: 0x%x != digest data: 0x%x", i, rsa_sig_digest[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_RSA2048, 0);
    //CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_LITTLE_ENDIAN, 1);

    // PKCS1 padding
    systime_1 = SysTick_Value();
    srand(systime_1);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_PKCS1);
    CRYPTO_RSA_Sign_Signature(CRYPTO0_Handler, rsa_sig_digest, sizeof(rsa_sig_digest),
            crypto_rsa2048_n, crypto_rsa2048_p, encrypto_aes_result_data_32w);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    CLOGD("I: RSA SIGN with PKCS1 padding finish!, use time: %dus", systime_diff/CRYPTO_MAIN_FREQ);
    i = 0;
    int err_count = 0;
    for(i = 0; i < 64; i++){
        if(crypto_rsa2048_pkcs1_signature[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, crypto_rsa2048_pkcs1_signature[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, crypto_rsa2048_pkcs1_signature[i], encrypto_aes_result_data_32w[i]);
        }
    }

//    CLOGD("I: RSA SIGN with PKCS1 padding finish!, signature result, use time: %dus", systime_diff/CRYPTO_MAIN_FREQ);
//
//    for(i = 0; i < 2048/32; i+=4){
//        CLOGD("0x%08x, 0x%08x, 0x%08x, 0x%08x, ", encrypto_aes_result_data_32w[i], encrypto_aes_result_data_32w[i+1], encrypto_aes_result_data_32w[i+2], encrypto_aes_result_data_32w[i+3]);
//    }

    systime_1 = SysTick_Value();

    int res = CRYPTO_RSA_Verify_Signature(CRYPTO0_Handler, rsa_sig_digest, sizeof(rsa_sig_digest),
            crypto_rsa2048_n, crypto_rsa_e_bigendian, crypto_rsa2048_pkcs1_signature);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    CLOGD("I: RSA Signature verify with PKCS1 padding finish!, result: %d(%s), use time: %dus", res, res==CSK_DRIVER_OK?"Success":"Failure", systime_diff/CRYPTO_MAIN_FREQ);

    // X931 padding
    systime_1 = SysTick_Value();
    srand(systime_1);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_X931);
    CRYPTO_RSA_Sign_Signature(CRYPTO0_Handler, rsa_sig_digest, sizeof(rsa_sig_digest),
            crypto_rsa2048_n, crypto_rsa2048_p, encrypto_aes_result_data_32w);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    CLOGD("I: RSA SIGN with X931 padding finish!, use time: %dus", systime_diff/CRYPTO_MAIN_FREQ);
    i = 0;
    err_count = 0;
    for(i = 0; i < 64; i++){
        if(crypto_rsa2048_x931_signature[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, crypto_rsa2048_x931_signature[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, crypto_rsa2048_x931_signature[i], encrypto_aes_result_data_32w[i]);
        }
    }

//    CLOGD("I: RSA SIGN with X931 padding finish!, signature result, use time: %dus", systime_diff/CRYPTO_MAIN_FREQ);
//
//    for(i = 0; i < 2048/32; i+=4){
//        CLOGD("0x%08x, 0x%08x, 0x%08x, 0x%08x, ", encrypto_aes_result_data_32w[i], encrypto_aes_result_data_32w[i+1], encrypto_aes_result_data_32w[i+2], encrypto_aes_result_data_32w[i+3]);
//    }

    systime_1 = SysTick_Value();

    res = CRYPTO_RSA_Verify_Signature(CRYPTO0_Handler, rsa_sig_digest, sizeof(rsa_sig_digest),
            crypto_rsa2048_n, crypto_rsa_e_bigendian, crypto_rsa2048_x931_signature);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    CLOGD("I: RSA Signature verify with X931 padding finish!, result: %d(%s), use time: %dus", res, res==CSK_DRIVER_OK?"Success":"Failure", systime_diff/CRYPTO_MAIN_FREQ);

    // PSS padding
    systime_1 = SysTick_Value();
    srand(systime_1);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_RSA_PADDING_MODE, CSK_CRYPTO_RSA_PADDING_PSS);
    CRYPTO_RSA_Sign_Signature(CRYPTO0_Handler, rsa_sig_digest, sizeof(rsa_sig_digest),
            crypto_rsa2048_n, crypto_rsa2048_p, encrypto_aes_result_data_32w);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    CLOGD("I: RSA SIGN with PSS padding finish!, signature result, use time: %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 2048/32; i+=4){
        CLOGD("0x%08x, 0x%08x, 0x%08x, 0x%08x, ", encrypto_aes_result_data_32w[i], encrypto_aes_result_data_32w[i+1], encrypto_aes_result_data_32w[i+2], encrypto_aes_result_data_32w[i+3]);
    }

    systime_1 = SysTick_Value();

    res = CRYPTO_RSA_Verify_Signature(CRYPTO0_Handler, rsa_sig_digest, sizeof(rsa_sig_digest),
            crypto_rsa2048_n, crypto_rsa_e_bigendian, crypto_rsa2048_pss_signature);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    CLOGD("I: RSA Signature verify with PSS padding finish!, result: %d(%s), use time: %dus", res, res==CSK_DRIVER_OK?"Success":"Failure", systime_diff/CRYPTO_MAIN_FREQ);


    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);

}

static uint32_t modexp512_a[] = {
        0xBDAE97A2,
        0x71616081,
        0x7CA4E7C6,
        0x6F8B930B,
        0x86BB8678,
        0xECB84CD5,
        0x7B3F0570,
        0x37971108,
        0xCF75256B,
        0xCE3C4605,
        0x74FBB659,
        0xAB48D25F,
        0x4B586288,
        0x941C60E2,
        0x96438E63,
        0x1929B1F6,

};

static uint32_t modexp512_e[] = {
        0xD95CEFA8,
        0x8A9C5966,
        0xBC80D6F9,
        0x69E38C12,
        0xCC7ECE24,
        0xFF95E3C1,
        0x3C149909,
        0xE31A55C8,
        0x60DD08C5,
        0xF4C5C5B2,
        0x12FF8B4C,
        0x9D7AFCB2,
        0xC36028DE,
        0xB781A7E1,
        0xDB7BB4EB,
        0xd9e7a4F6,

};

static uint32_t modexp512_mod[] = {
        0x385A64A3,
        0xF5695F0C,
        0x9E8E7C0D,
        0x3AD6ED6B,
        0x9CA3A397,
        0xB566A61D,
        0x1DB77E7E,
        0x06F18FEB,
        0x8B930117,
        0x3B1D112D,
        0x1363E1F4,
        0x15A9CE89,
        0x42881719,
        0xA82559BA,
        0x1EED5249,
        0x3085F56A,
};

static uint32_t modexp512_result[] = {
         0x1ec91544,
         0x260fa79f,
         0x13d945cb,
         0x8822e537,
         0x9699fc6f,
         0x0b318128,
         0x9b2ac9a8,
         0x033db460,
         0x35ff88ea,
         0x7ccde341,
         0x5dbfbb09,
         0x1e5084e5,
         0x77476a4b,
         0x75bf7480,
         0x9f6fa57b,
         0x2bf78dc9,
};

static uint32_t modexp224_mod[] = {
        0x7ec8c0ff,
        0x97da89f5,
        0xb09f0757,
        0x75d1d787,
        0x2a183025,
        0x26436686,
        0xd7c134aa
};

static uint32_t modexp224_result[] = {
        0xb442318a,
        0xaec4ba8d,
        0xfa085404,
        0x99ff6866,
        0x4a2b77e3,
        0xc88bd8c0,
        0x71299ecb,
        0,
};

void CRYPTO0_MOD_EXP_Little_Endian()
{

    int i = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_LITTLE_ENDIAN, 1);
    systime_1 = SysTick_Value();

    CRYPTO_Mod_Operate(CRYPTO0_Handler, CRYPTO_MOD_EXP, 64, encrypto_aes_result_data_32w, modexp512_a, modexp512_e, modexp512_mod);
    //crypto_mod_exp(CRYPTO0_Handler, encrypto_aes_result_data_32w, NULL, modexp512_a, 64, modexp512_e, 64, modexp512_mod, 64);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("Mod Exp 512 test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 16; i++){
        if(encrypto_aes_result_data_32w[i] == modexp512_result[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, modexp512_result[i], encrypto_aes_result_data_32w[i]);
        }
    }

    systime_1 = SysTick_Value();

    CRYPTO_Mod_Operate(CRYPTO0_Handler, CRYPTO_MOD_EXP, 28, encrypto_aes_result_data_32w, modexp512_a, modexp512_e, modexp224_mod);
    //crypto_mod_exp(CRYPTO0_Handler, encrypto_aes_result_data_32w, NULL, modexp512_a, 28, modexp512_e, 28, modexp224_mod, 28);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("Mod Exp 224 test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 8; i++){
        if(encrypto_aes_result_data_32w[i] == modexp224_result[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, modexp224_result[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_LITTLE_ENDIAN, 0);
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
}

// write efuse and flash before the test
// write efuse: tools/uart_burn_tool/Uart_Burn_Tool.exe -b 115200 -p COM6 -u efuse -f src/drv_demo/drv_demo_arcs/crypto/sec_rsa/test_efuse.bin
// write flash: tools/uart_burn_tool/Uart_Burn_Tool.exe -b 115200 -p COM6 -f src/drv_demo/drv_demo_arcs/crypto/sec_rsa/test_flash.bin -m -d -a 0x0
void CRYPTO0_ECC_RSA2048_Flash_Verify_Signature()
{
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;
    ls_ota_header_t *hdr = (ls_ota_header_t*)CMN_FLASH_REGION;
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    systime_1 = SysTick_Value();
    int res = CRYPTO_Verify_Flash_Signature(CRYPTO0_Handler, hdr, OTA_SIGN_RSA2048);
    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
    CLOGD("Flash verify finished, result: %d(%s), use time: %dus", res, res==CSK_DRIVER_OK?"Success":"Failure", systime_diff/CRYPTO_MAIN_FREQ);

}
