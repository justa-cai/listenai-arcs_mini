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

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((aligned(32))) static const unsigned char sha_raw_str[] =
"To be, or not to be, that is the question,\n"
"Whether tis nobler in the minde to suffer\n"
"The ſlings and arrowes of outragious fortune,\n"
"Or to take Armes again in a sea of troubles,\n"
"And by opposing, end them, to die to sleep;\n"
"No more, and by a sleep, to say we end\n"
"The heart-ache, and the thousand natural shocks\n"
"That flesh is heir to? tis a consumation\n"
"Devoutly to be wished. To die to sleep,\n"
"To sleepe, perchance to dreame, Aye, there's the rub,\n"
"For in that sleep of death what dreams may come\n"
"When we haue shuffled off this mortal coil\n"
"Must give us pause. There's the respect\n"
"That makes calamity of so long life:\n"
"For who would bear the Ships and Scorns of time,\n"
"The oppressor's wrong, the proud man's Contumely,\n"
"The pangs of dispised love, the Law's delay,\n"
;

uint32_t encrypto_hash_source_data_512 [] = {
        0x84C05470,
        0x84C05470,
        0x565E2C3A,
        0x365DAC0B,
        0xAFEAC4B6,
        0xAFEAC4B6,
        0xD39CB5D3,
        0x80EB3170,
        0xF32D4FA4,
        0xF32D4FA4,
        0xC1F57468,
        0x1B001E7B,
        0x7FD9BD70,
        0x7FD9BD70,

};

uint32_t encrypto_hash_result_data_512[] = {
        0x9498c00a,
        0x65d5776f,
        0x0e94ba4a,
        0x0288412e,
        0x25b24eb5,
        0xf1f52ea8,
        0xff6bc49a,
        0xec073741,
        0xe706d12b,
        0x4abee2c1,
        0xf6128c02,
        0x3ae363e9,
        0x53e7f169,
        0x02e51531,
        0x29baa892,
        0xb425470c,

};


void CRYPTO0_SHA512_LittleEndian_Test(){

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    volatile uint32_t* source_addr = encrypto_hash_source_data_512;
    volatile uint32_t* dst_addr = encrypto_aes_result_data_32w;

    //CRYPTO_SWAP_Bytes(CRYPTO0_Handler, source_addr, 56, source_addr);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA512);

    // 1th loop
    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 56, (uint32_t*)dst_addr, 0);

    int i = 0;

    for(i = 0; i < 16; i++){
        if(encrypto_hash_result_data_512[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_hash_result_data_512[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_hash_result_data_512[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

static uint32_t encrypto_hash_reference_data_32w[40] = {
        0x806cc0ec,
        0x5952a589,
        0x4b49e0e3,
        0xa1570014,
        0x8c7d3c88,
        0xea554b6f,
        0xce2eb0d6,
        0x7e882eb8,

        0xbf1678ba,
        0xeacf018f,
        0xde404141,
        0x2322ae5d,
        0xa36103b0,
        0x9c7a1796,
        0x61ff10b4,
        0xad1500f2,
        0x616a8d24,
        0xb83806d2,
        0x9326c0e5,
        0x39603e0c,
        0x59e43ca3,
        0x6721ff64,
        0xd4edecf6,
        0xc106db19,
        0xa2448da8,
        0x2f3a0a94,
        0x493063c3,
        0xbf63d226,
        0x56fb1a27,
        0x4056ab2b,
        0xf5810ecb,
        0xa32043e8,
};

static uint32_t encrypto_hash_source_data_96w[] = {
        0x84C05470,
        0x84C05470,
        0x565E2C3A,
        0x365DAC0B,
        0xAFEAC4B6,
        0xAFEAC4B6,
        0xD39CB5D3,
        0x80EB3170,
        0xF32D4FA4,
        0xF32D4FA4,
        0xC1F57468,
        0x1B001E7B,
        0x7FD9BD70,
        0x7FD9BD70,
    0x80636261, // 1 loop
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000018,
    0x64636261, // 2 loop
    0x65646362,
    0x66656463,
    0x67666564,
    0x68676665,
    0x69686766,
    0x6a696867,
    0x6b6a6968,
    0x6c6b6a69,
    0x6d6c6b6a,
    0x6e6d6c6b,
    0x6f6e6d6c,
    0x706f6e6d,
    0x71706f6e,
    0x00000080,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x000001c0,
    0x61616161, // 3 loop
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x80616161,
    0x00000000,
    0xb8050000,
};

void CRYPTO0_SHA256_LittleEndian_Test(){

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    volatile uint32_t* source_addr = encrypto_hash_source_data_96w;
    volatile uint32_t* dst_addr = encrypto_aes_result_data_32w;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    systime_1 = SysTick_Value();

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA256);

    // 0th loop
    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 56, (uint32_t*)dst_addr, 0);

    source_addr += 0x0e;
    dst_addr += 0x8;

    // 1th loop

    //CRYPTO_SWAP_Bytes(CRYPTO0_Handler, source_addr, 64, source_addr);

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 3, (uint32_t*)dst_addr, 0);

    source_addr += 0x10;
    dst_addr += 0x8;

    // 2th loop
    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 56, (uint32_t*)dst_addr, 0);

    source_addr += 0x20;
    dst_addr += 0x8;

    // 4th loop
    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 64, NULL, 0);

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)(source_addr + 0x10), 64, NULL, 1);

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)(source_addr + 0x20), 55, (uint32_t*)dst_addr, 1);

    int i = 0;
    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("SHA256 test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 32; i++){
        if(encrypto_hash_reference_data_32w[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_hash_reference_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_hash_reference_data_32w[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

static uint32_t encrypto_hash_sha1_reference_data[] = {
        0x3874c1a7,
        0x59f0123c,
        0xa5eef49f,
        0x932c9ea3,
        0x2c816613,
};

void CRYPTO0_SHA1_LittleEndian_Test(void){
    volatile uint32_t* source_addr = (uint32_t*)sha_raw_str;
    volatile uint32_t* dst_addr = encrypto_aes_result_data_32w;
    uint64_t r_len = sizeof(sha_raw_str);

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA1);

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, r_len, (uint32_t*)encrypto_aes_result_data_32w, 0);


    uint8_t i;
    for(i = 0; i < 5; i++){
        if(encrypto_hash_sha1_reference_data[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_hash_sha1_reference_data[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_hash_sha1_reference_data[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

__attribute__((aligned(32))) static volatile uint32_t encrypto_hash_source_data_16w[16] = {
    0xbd,
};

__attribute__((aligned(32))) static uint32_t crypto_sha_result_data_8w[16] = {0};

__attribute__((aligned(32))) static uint32_t crypto_sha_reference_data_8w[8] = {
        0x20573268,
        0x827cbdaa,
        0x4b550ff3,
        0x70053d31,
        0xbbcc5ac9,
        0xaab5c47d,
        0xc00412e1,
        0x2b73fe8f,
};

static uint32_t encrypto_hash_sha224_reference_data[] = {
        0xdda67304,
        0x71c1b466,
        0xed625cd4,
        0xeadd5dc6,
        0xa0c8b8ba,
        0x08db52dd,
        0x368a6e43,
};

void CRYPTO0_SHA224_LittleEndian_Test(void){
    volatile uint32_t* source_addr = (uint32_t*)sha_raw_str;
    volatile uint32_t* dst_addr = encrypto_aes_result_data_32w;
    uint64_t r_len = sizeof(sha_raw_str);

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA224);

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, r_len, (uint32_t*)dst_addr, 0);

    uint8_t i;
    for(i = 0; i < 7; i++){
        if(encrypto_hash_sha224_reference_data[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_hash_sha224_reference_data[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

#include "test_data.h"

static uint32_t encrypto_hash_sha224_longstream_reference_data[] = {
    0x6a23087b,
    0x681c79a5,
    0xc467e7c1,
    0xfb7de092,
    0xe6abc381,
    0x883edb9a,
    0xfc4b4184,
};

void CRYPTO0_SHA224_LittleEndian_LongStream_Test(void){
    volatile uint32_t* source_addr = (volatile uint32_t*)crypto_long_validation_array;
    volatile uint32_t* dst_addr = crypto_sha_result_data_8w;
    uint64_t r_len=sizeof(crypto_long_validation_array);

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    systime_1 = SysTick_Value();
    // padding is done in hw module
    // source_addr = Hash256_Padding((char*)source_addr, sizeof(crypto_long_validation_array), &r_len);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA224);

    uint8_t i;

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 64, NULL, 0);

    source_addr += 0x10;

    for(i = 0; i < (r_len/64) - 1; i++){
        CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 64, NULL, 1);
        source_addr += 0x10;

    }

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, r_len%64, (uint32_t*)dst_addr, 1);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("ShA256 longstream test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 7; i++){
        if(encrypto_hash_sha224_longstream_reference_data[i] == crypto_sha_result_data_8w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_hash_sha224_longstream_reference_data[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_hash_sha224_longstream_reference_data[i], crypto_sha_result_data_8w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

void CRYPTO0_SHA256_LittleEndian_LongStream_Light_Test(void){
    volatile uint32_t* source_addr = (volatile uint32_t*)crypto_long_validation_array;
    volatile uint32_t* dst_addr = crypto_sha_result_data_8w;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    uint32_t data_len = sizeof(crypto_long_validation_array);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA256);

    uint8_t i;

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, data_len, (uint32_t*)dst_addr, 0);

    for(i = 0; i < 8; i++){
        if(crypto_sha256_long_ref_arrary[i] == crypto_sha_result_data_8w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, crypto_sha256_long_ref_arrary[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, crypto_sha256_long_ref_arrary[i], crypto_sha_result_data_8w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

uint32_t encrypto_hash384_source_data[] = {
        0x84C05470,
        0x84C05470,
        0x565E2C3A,
        0x365DAC0B,
        0xAFEAC4B6,
        0xAFEAC4B6,
        0xD39CB5D3,
        0x80EB3170,
        0xF32D4FA4,
        0xF32D4FA4,
        0xC1F57468,
        0x1B001E7B,
        0x7FD9BD70,
        0x7FD9BD70,

};

uint32_t encrypto_hash384_result_data[] = {
        0x7bd2975d,
        0x57d64d38,
        0x6b7b562a,
        0x192b9403,
        0xd9db226a,
        0xe735bc25,
        0x199a5c1f,
        0x46df2967,
        0xbd6afd21,
        0xa064bf71,
        0x32b15ad9,
        0x2b467bc0,

};

void CRYPTO0_SHA384_LittleEndian_Test(){

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    volatile uint32_t* source_addr = encrypto_hash384_source_data;
    volatile uint32_t* dst_addr = encrypto_aes_result_data_32w;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA384);

    // 1th loop
    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 56, (uint32_t*)dst_addr, 0);

    int i = 0;

    for(i = 0; i < 12; i++){
        if(encrypto_hash384_result_data[i] == dst_addr[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_hash384_result_data[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_hash384_result_data[i], dst_addr[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

//#define SHA_VALUE_CHECK
void CRYPTO0_SHA384_LittleEndian_LongStream_Test(void){
    volatile uint32_t* source_addr = (volatile uint32_t*)crypto_long_validation_array;
    volatile uint32_t* dst_addr = crypto_sha_result_data_8w;
    uint64_t r_len=sizeof(crypto_long_validation_array);
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA384);
    systime_1 = SysTick_Value();
    uint8_t i,j;

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 128, NULL, 0);
#ifdef SHA_VALUE_CHECK
    CRYPTO_Get_Hash(CRYPTO0_Handler, dst_addr);
    CLOGD("Fist segment result:");
    for(i = 0; i < 12; i++){
         CLOGD("I:data: 0x%x", dst_addr[i]);
    }
#endif
    source_addr += 0x20;

    for(i = 0; i < (r_len/128) - 1; i++){
        CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 128, NULL, 1);
#ifdef SHA_VALUE_CHECK
        CRYPTO_Get_Hash(CRYPTO0_Handler, dst_addr);
        CLOGD("%d segment result:", i);
        for(j = 0; j < 12; j++){
             CLOGD("I:data: 0x%x", dst_addr[j]);
        }
#endif
        source_addr += 0x20;
    }

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, r_len%128, (uint32_t*)dst_addr, 1);
#ifdef SHA_VALUE_CHECK
    CLOGD("last segment result:");
    for(i = 0; i < 12; i++){
         CLOGD("I:data: 0x%x", dst_addr[i]);
    }
#endif
    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("SHA384 longstream test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 12; i++){
        if(crypto_sha384_long_ref_arrary[i] == crypto_sha_result_data_8w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, crypto_sha384_long_ref_arrary[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, crypto_sha384_long_ref_arrary[i], crypto_sha_result_data_8w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

void CRYPTO0_SHA512_LittleEndian_LongStream_Test(void){
    volatile uint32_t* source_addr = (volatile uint32_t*)crypto_long_validation_array;
    volatile uint32_t* dst_addr = crypto_sha_result_data_8w;
    uint64_t r_len=sizeof(crypto_long_validation_array);
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    systime_1 = SysTick_Value();

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA512);

    uint8_t i;

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 256, NULL, 0);

    source_addr += 0x40;

    for(i = 0; i < (r_len/256) - 1; i++){
        CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 256, NULL, 1);
        source_addr += 0x40;
    }

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, r_len%256, (uint32_t*)dst_addr, 1);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    CLOGD("SHA512 longstream test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 16; i++){
        if(crypto_sha512_long_ref_arrary[i] == crypto_sha_result_data_8w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, crypto_sha512_long_ref_arrary[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, crypto_sha512_long_ref_arrary[i], crypto_sha_result_data_8w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}


__attribute__((aligned(32))) static unsigned char key_mac[] = {
    0x25, 0xfd, 0x12, 0x99, 0xdf, 0xad, 0x1a, 0x03,
    0x0a, 0x81, 0x3c, 0x2d, 0xcc, 0x05, 0xd1, 0x5c,
    0x17, 0x7a, 0x36, 0x73, 0x17, 0xef, 0x41, 0x75,
    0x71, 0x18, 0xe0, 0x1a, 0xda, 0x99, 0xc3, 0x61,
    0x38, 0xb5, 0xb1, 0xe0, 0x82, 0x2c, 0x70, 0xa4,
    0xc0, 0x8e, 0x5e, 0xf9, 0x93, 0x9f, 0xcf, 0xf7,
    0x32, 0x4d, 0x0c, 0xbd, 0x31, 0x12, 0x0f, 0x9a,
    0x15, 0xee, 0x82, 0xdb, 0x8d, 0x29, 0x54, 0x14,
};

static const uint32_t hmac_sha1_dest_result[] =
{
    0x0df29d1a,
    0xb84d663d,
    0x87c054eb,
    0x5ef86d10,
    0x8831df60,
};

void CRYPTO0_HMAC_SHA1_LittleEndian_Test(void)
{
    volatile uint32_t* source_addr = encrypto_hash_source_data_16w;
    volatile uint32_t* dst_addr = crypto_sha_result_data_8w;
    uint64_t r_len = 1;
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    systime_1 = SysTick_Value();
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA1);

    CRYPTO_HMAC(CRYPTO0_Handler, (uint32_t*)sha_raw_str, sizeof(sha_raw_str), (uint32_t*)key_mac, sizeof(key_mac), (uint32_t*)dst_addr);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;
    CLOGD("HMAC SHA1 test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    uint8_t i;
    for(i = 0; i < 5; i++){
        if(hmac_sha1_dest_result[i] == crypto_sha_result_data_8w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, hmac_sha1_dest_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, hmac_sha1_dest_result[i], crypto_sha_result_data_8w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

static const uint32_t hmac_sha256_dest_result[] =
{
    0x9e7c1434,
    0x066f7d00,
    0xb986f684,
    0xd085d4ea,
    0xb5e863bd,
    0xa1adf773,
    0x8fc090ab,
    0x26c2b9da,
};

void CRYPTO0_HMAC_SHA256_LittleEndian_Test(void)
{
    volatile uint32_t* source_addr = encrypto_hash_source_data_16w;
    volatile uint32_t* dst_addr = crypto_sha_result_data_8w;
    uint64_t r_len = 1;
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    systime_1 = SysTick_Value();
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA256);

    CRYPTO_HMAC(CRYPTO0_Handler, (uint32_t*)sha_raw_str, sizeof(sha_raw_str), (uint32_t*)key_mac, sizeof(key_mac), (uint32_t*)dst_addr);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    CLOGD("HMAC SHA256 test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    uint8_t i;
    for(i = 0; i < 8; i++){
        if(hmac_sha256_dest_result[i] == crypto_sha_result_data_8w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, hmac_sha256_dest_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, hmac_sha256_dest_result[i], crypto_sha_result_data_8w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

static const uint32_t hmac_sha512_dest_result[] =
{
        0x1a9ad6b0, 0x56ad04a9, 0xe545b00f, 0xc52d96fc, 0xe69a530e, 0x3562a044, 0xaf614c6a, 0xba6b60de,
        0x4cd14ef2, 0xacdc5872, 0xd5a0f719, 0x9b32ccd1, 0xc57523c7, 0x2cc77d25, 0xbfa4ea33, 0x4a2c4311,
};

void CRYPTO0_HMAC_SHA512_LittleEndian_Test(void)
{
    volatile uint32_t* source_addr = encrypto_hash_source_data_16w;
    volatile uint32_t* dst_addr = crypto_sha_result_data_8w;
    uint64_t r_len = 1;
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    systime_1 = SysTick_Value();
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA512);

    CRYPTO_HMAC(CRYPTO0_Handler, (uint32_t*)sha_raw_str, sizeof(sha_raw_str), (uint32_t*)key_mac, sizeof(key_mac), (uint32_t*)dst_addr);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    CLOGD("HMAC SHA512 test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    uint8_t i;
    for(i = 0; i < 16; i++){
        if(hmac_sha512_dest_result[i] == crypto_sha_result_data_8w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, hmac_sha512_dest_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, hmac_sha512_dest_result[i], crypto_sha_result_data_8w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

static const uint32_t hmac_sha224_dest_result[] =
{
        0x3b53dc64,
        0xedcfaa6a,
        0xd1abcce4,
        0x91aa0a24,
        0xce93c55f,
        0x4d820301,
        0x865ccca8,
};

void CRYPTO0_HMAC_SHA224_LittleEndian_LongStream_Test(void)
{
    volatile uint32_t* source_addr = (uint32_t*)crypto_long_validation_array;
    volatile uint32_t* dst_addr = crypto_sha_result_data_8w;
    uint64_t r_len = sizeof(crypto_long_validation_array);

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA224);

    CRYPTO_HMAC(CRYPTO0_Handler, (uint32_t*)source_addr, r_len, (uint32_t*)key_mac, sizeof(key_mac), (uint32_t*)dst_addr);

    uint8_t i;
    for(i = 0; i < 7; i++){
        if(hmac_sha224_dest_result[i] == crypto_sha_result_data_8w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, hmac_sha224_dest_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, hmac_sha224_dest_result[i], crypto_sha_result_data_8w[i]);
        }
    }

    // segment hmac
    CRYPTO_HMAC(CRYPTO0_Handler, (uint32_t*)source_addr, 256, (uint32_t*)key_mac, sizeof(key_mac), NULL);

    source_addr += 0x40;

    for(i = 0; i < (r_len/256) - 1; i++){
        CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 256, NULL, 1);

        source_addr += 0x40;
    }

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, r_len%256, (uint32_t*)dst_addr, 1);

    for(i = 0; i < 7; i++){
        if(hmac_sha224_dest_result[i] == crypto_sha_result_data_8w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, hmac_sha224_dest_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, hmac_sha224_dest_result[i], crypto_sha_result_data_8w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

static const uint32_t hmac_sha384_dest_result[] =
{
        0x073305de,
        0x4771510d,
        0xe58f87d7,
        0x8efe8262,
        0x5c1cfe75,
        0x400021e7,
        0x5b630272,
        0xd9384419,
        0x29abfc30,
        0xb554a696,
        0x4324ba12,
        0x52e5e73e,
};

void CRYPTO0_HMAC_SHA384_LittleEndian_LongStream_Test(void)
{
    volatile uint32_t* source_addr = (uint32_t*)crypto_long_validation_array;
    volatile uint32_t* dst_addr = crypto_sha_result_data_8w;
    uint64_t r_len = sizeof(crypto_long_validation_array);

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA384);

    CRYPTO_HMAC(CRYPTO0_Handler, (uint32_t*)source_addr, r_len, (uint32_t*)key_mac, sizeof(key_mac), (uint32_t*)dst_addr);

    uint8_t i;
    for(i = 0; i < 12; i++){
        if(hmac_sha384_dest_result[i] == crypto_sha_result_data_8w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, hmac_sha384_dest_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, hmac_sha384_dest_result[i], crypto_sha_result_data_8w[i]);
        }
    }

    // segment hmac
    CRYPTO_HMAC(CRYPTO0_Handler, (uint32_t*)source_addr, 256, (uint32_t*)key_mac, sizeof(key_mac), NULL);

    source_addr += 0x40;

    for(i = 0; i < (r_len/256) - 1; i++){
        CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, 256, NULL, 1);

        source_addr += 0x40;
    }

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)source_addr, r_len%256, (uint32_t*)dst_addr, 1);

    for(i = 0; i < 12; i++){
        if(hmac_sha384_dest_result[i] == crypto_sha_result_data_8w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, hmac_sha384_dest_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, hmac_sha384_dest_result[i], crypto_sha_result_data_8w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);

}
