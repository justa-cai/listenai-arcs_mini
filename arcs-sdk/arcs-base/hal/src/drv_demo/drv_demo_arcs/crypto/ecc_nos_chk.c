/*
 * crypto_nos_chk.c
 *
 *  Created on: 2020/10/9
 *      Author: USER
 */
#include "dbg_assert.h"
#include "chip.h"
#include "Driver_CRYPTO.h"
#include "crypto_nos_chk.h"
#include "log_print.h"
#include "systick.h"
#include "ota.h"

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t ecc_test_temp_buff1[32];
static uint32_t ecc_test_temp_buff2[32];
static uint32_t ecc_test_private_key1[16];
static uint32_t ecc_test_private_key2[16];
static uint32_t ecc_test_public_key1[32];
static uint32_t ecc_test_public_key2[32];

static uint32_t modadd512_a[] = {
        0x85478F90,
        0x70C30D2E,
        0x8C51F1A5,
        0xA589341F,
        0x9E6AD0B2,
        0x5BEE7A3B,
        0xB9B80247,
        0x9D973CDF,
        0x0999DB10,
        0x1DD6146C,
        0xF4C1BB14,
        0xC06A4A75,
        0x898571B8,
        0xCCF7C502,
        0xD918D4DD,
        0xE6694A61,
};


static uint32_t modadd512_b[] = {
        0x08AE37FF,
        0xD4E52D38,
        0xF577D108,
        0x0007CF6C,
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
};

static uint32_t modadd512_mod[] = {
        0x1DA1070B,
        0x5FAD0305,
        0x479D6457,
        0x99E4A656,
        0xE7A5E603,
        0x3A22BF9C,
        0xE6026574,
        0x2D65515E,
        0x654AAD1D,
        0x4164AD54,
        0x8C4CC552,
        0x454C4A5A,
        0x18C79839,
        0xE6A80EC5,
        0xF814DC39,
        0xE5E8EFE8,
};

static uint32_t modadd512_result[] = {0x7054C084, 0xE5FB3761, 0x3A2C5E56, 0x0BAC5D36, 0xB6C4EAAF, 0x21CBBA9E, 0xD3B59CD3, 0x7031EB80,
                                      0xA44F2DF3, 0xDC716717, 0x6874F5C1, 0x7B1E001B, 0x70BDD97F, 0xE64FB63D, 0xE103F8A3, 0x00805A78
};


static uint32_t modsub512_a[] = {0x7054C084, 0xE5FB3761, 0x3A2C5E56, 0x0BAC5D36, 0xB6C4EAAF, 0x21CBBA9E, 0xD3B59CD3, 0x7031EB80,
        0xA44F2DF3, 0xDC716717, 0x6874F5C1, 0x7B1E001B, 0x70BDD97F, 0xE64FB63D, 0xE103F8A3, 0xf6805A78
};


static uint32_t modsub512_b[] = {
        0x08AE37FF,
        0xD4E52D38,
        0xF577D108,
        0x0007CF6C,
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
};

static uint32_t modsub512_mod[] = {
        0x1DA1070B,
        0x5FAD0305,
        0x479D6457,
        0x99E4A656,
        0xE7A5E603,
        0x3A22BF9C,
        0xE6026574,
        0x2D65515E,
        0x654AAD1D,
        0x4164AD54,
        0x8C4CC552,
        0x454C4A5A,
        0x18C79839,
        0xE6A80EC5,
        0xF814DC39,
        0x85E8EFE8,
};

static uint32_t modsub512_result[] = {
        0x4a05817a,
        0xb1690724,
        0xfd1728f6,
        0x71bfe772,
        0xcf1f04ab,
        0xe7a8fb01,
        0xedb3375e,
        0x42cc9a21,
        0x3f0480d6,
        0x9b0cb9c3,
        0xdc28306f,
        0x35d1b5c0,
        0x57f64146,
        0xffa7a778,
        0xe8ef1c69,
        0x70976a8f,
};


static uint32_t modmult512_b[] = {
        0x91A4D289,
        0x1CC2A3AD,
        0xC9B6B60B,
        0xD85530BB,
        0xADF19BDD,
        0x8C8B97D6,
        0xC9B6B60B,
        0xD85530BB,
        0xADF19BDD,
        0xC9B6B60B,
        0xD85530BB,
        0xADF19BDD,
        0xC9B6B60B,
        0xC9B6B60B,
        0xD85530BB,
        0xADF19BDD,
};

static uint32_t modmult512_a[] = {
        0x805318EA,
        0xBC4E9B96,
        0x7189F494,
        0x805318EA,
        0xBC4E9B96,
        0x7189F494,
        0x805318EA,
        0xBC4E9B96,
        0x7189F494,
        0x805318EA,
        0xBC4E9B96,
        0x7189F494,
        0x805318EA,
        0xBC4E9B96,
        0xBC4E9B96,
        0x7189F494,

};

static uint32_t modmult512_mod[] = {
        0x7BDA04F1,
        0x7678ED71,
        0x05A4A1F1,
        0x203C1C54,
        0xC048168C,
        0x125F5DE6,
        0x230A7D46,
        0xB9617BAD,
        0x4F17C799,
        0x3418DFAA,
        0x2449104C,
        0x7C09D339,
        0x5D60E7E1,
        0xA6793404,
        0x35759DA1,
        0xA63B9465,

};

static uint32_t modmult512_result[] =  {
        0x25f62421,
        0x89ef7815,
        0x2d741e26,
        0xff57b5eb,
        0xd6dcb327,
        0x49fee239,
        0xba80d1bc,
        0xf47cf0fa,
        0x64e94b0b,
        0x131bf8eb,
        0xa427984f,
        0x9b430deb,
        0x3f44c497,
        0xe0684ddd,
        0x0bd72d3a,
        0x89f6aeb9,


};


static uint32_t moddiv512_a[] = {
        0x6ADF6B5F,
        0x6300FA39,
        0xFC84D3B0,
        0x6FEFA706,
        0xBA9EB9AC,
        0x641A757B,
        0xE3C20199,
        0x0000074E,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,

};

static uint32_t moddiv512_b[] = {
        0x8DE66EAB,
        0x1A83A5E9,
        0x131B9AC6,
        0x4EAC0E1D,
        0x71060C42,
        0xDA5A8CB5,
        0x4D6CABFA,
        0x61267FE4,
        0xE2FE766D,
        0xC46F95C5,
        0xEC396BDE,
        0x097BCDE7,
        0x0000859B,
        0x00000000,
        0x00000000,
        0x00000000,

};

static uint32_t moddiv512_mod[] = {
        0xEEA6616F,
        0xF0902F99,
        0x58543EAA,
        0x2FD8556E,
        0x354A9A1F,
        0x4CF18BE8,
        0x1A03964D,
        0xBA9EA90E,
        0xB7808AB8,
        0x4CB48BB7,
        0x6466F455,
        0x9E017705,
        0x397909DD,
        0xD9E581DF,
        0x12484AA1,
        0xA5B3D9F6,

};

static uint32_t moddiv512_result[] = {
        0x283D0327,
        0x9111B8D9,
        0x3F8CD9A9,
        0xDF4CE11A,
        0x0DA91875,
        0x859A9D79,
        0xBA4F87C6,
        0x1B0FD08C,
        0x0603867D,
        0x60984EA4,
        0x9DFCBF27,
        0x1E258891,
        0x594D7E35,
        0x49303BB8,
        0x86B10B0F,
        0x813B2D07,

};


static uint32_t modinv512_a[] = {
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
        0x000004F6,

};


static uint32_t modinv512_mod[] = {
        0xF85A64A3,
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
        0xC085F56A,

};

static uint32_t modinv512_result[] = {
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


static uint32_t modmod512_a[] = {
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
        0x59e7a4F6,

};


static uint32_t modmod512_mod[] = {
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

static uint32_t modmod512_result[] = {
        0xa1028b05,
        0x9532fa5a,
        0x1df25aeb,
        0x2f0c9ea7,
        0x2fdb2a8d,
        0x4a2f3da4,
        0x1e5d1a8b,
        0xdc28c5dd,
        0xd54a07ae,
        0xb9a8b484,
        0xff9ba958,
        0x87d12e28,
        0x80d811c5,
        0x0f5c4e27,
        0xbc8e62a2,
        0x2961af8c,
};

void CRYPTO0_MOD_OPERATE_512_LittleEndian_Test(void){

    int i = 0;
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_LITTLE_ENDIAN, 1);

    systime_1 = SysTick_Value();

    CRYPTO_Mod_Operate(CRYPTO0_Handler, CRYPTO_MOD_ADD, 64, ecc_test_temp_buff1, modadd512_b, modadd512_a, modadd512_mod);


    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("Mod Add 512 test finished, use time %dus:", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 16; i++){
        if(ecc_test_temp_buff1[i] == modadd512_result[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, modadd512_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, modadd512_result[i], ecc_test_temp_buff1[i]);
        }
    }

    systime_1 = SysTick_Value();

    CRYPTO_Mod_Operate(CRYPTO0_Handler, CRYPTO_MOD_SUB, 64, ecc_test_temp_buff1, modsub512_a, modsub512_b, modsub512_mod);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;
    CLOGD("Mod sub 512 test finished, use time %dus:", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 16; i++){
        if(ecc_test_temp_buff1[i] == modsub512_result[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, modsub512_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, modsub512_result[i], ecc_test_temp_buff1[i]);
        }
    }

    systime_1 = SysTick_Value();

    CRYPTO_Mod_Operate(CRYPTO0_Handler, CRYPTO_MOD_MULT, 64, ecc_test_temp_buff1, modmult512_b, modmult512_a, modmult512_mod);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;
    CLOGD("Mod Mult 512 test finished, use time %dus:", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 16; i++){
        if(ecc_test_temp_buff1[i] == modmult512_result[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, modmult512_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, modmult512_result[i], ecc_test_temp_buff1[i]);
        }
    }

    systime_1 = SysTick_Value();

    CRYPTO_Mod_Operate(CRYPTO0_Handler, CRYPTO_MOD_DIV, 64, ecc_test_temp_buff1, moddiv512_a, moddiv512_b, moddiv512_mod);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;
    CLOGD("Mod Div 512 test finished, use time %dus:", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 16; i++){
        if(ecc_test_temp_buff1[i] == moddiv512_result[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, moddiv512_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, moddiv512_result[i], ecc_test_temp_buff1[i]);
        }
    }

    systime_1 = SysTick_Value();

    CRYPTO_Mod_Operate(CRYPTO0_Handler, CRYPTO_MOD_INV, 64, ecc_test_temp_buff1, modinv512_a, NULL, modinv512_mod);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;
    CLOGD("Mod INV 512 test finished, use time %dus:", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 16; i++){
        if(ecc_test_temp_buff1[i] == modinv512_result[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, modinv512_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, modinv512_result[i], ecc_test_temp_buff1[i]);
        }
    }

    systime_1 = SysTick_Value();

    CRYPTO_Mod_Operate(CRYPTO0_Handler, CRYPTO_MOD_MOD, 64, ecc_test_temp_buff1, modmod512_a, NULL, modmod512_mod);


    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;
    CLOGD("Mod mod 512 test finished, use time %dus:", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 16; i++){
        if(ecc_test_temp_buff1[i] == modmod512_result[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, modmod512_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, modmod512_result[i], ecc_test_temp_buff1[i]);
        }
    }
    // restore default endian
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_LITTLE_ENDIAN, 0);
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
}

void CRYPTO0_ECC_P192_Generate_Key()
{
    int i = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P192);

    CRYPTO_ECC_Generate_Key(CRYPTO0_Handler, 0, ecc_test_private_key1, ecc_test_public_key1);


    CLOGD("ECC192 key generated, private key:");
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_private_key1[0], ecc_test_private_key1[1], ecc_test_private_key1[2],
                                           ecc_test_private_key1[3], ecc_test_private_key1[4], ecc_test_private_key1[5]);
    CLOGD("public key:");
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_public_key1[0], ecc_test_public_key1[1], ecc_test_public_key1[2],
                                           ecc_test_public_key1[3], ecc_test_public_key1[4], ecc_test_public_key1[5]);
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_public_key1[6], ecc_test_public_key1[7], ecc_test_public_key1[8],
                                           ecc_test_public_key1[9], ecc_test_public_key1[10], ecc_test_public_key1[11]);

    //ecc_test_public_key1[0]=2; // test error
    int res = CRYPTO_ECC_Verify_Key(CRYPTO0_Handler, ecc_test_public_key1);

    CLOGD("ECC192 key verify, result: %d(%s)", res, res==CSK_DRIVER_OK?"Success":"Failure");

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);

}

void CRYPTO0_ECC_P224_Generate_Key()
{
    int i = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P224);


    CRYPTO_ECC_Generate_Key(CRYPTO0_Handler, 0, ecc_test_private_key1, ecc_test_public_key1);

    CLOGD("ECC224 key generated, private key:");
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_private_key1[0], ecc_test_private_key1[1], ecc_test_private_key1[2],
                                           ecc_test_private_key1[3], ecc_test_private_key1[4], ecc_test_private_key1[5], ecc_test_private_key1[6]);
    CLOGD("public key:");
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_public_key1[0], ecc_test_public_key1[1], ecc_test_public_key1[2],
                                           ecc_test_public_key1[3], ecc_test_public_key1[4], ecc_test_public_key1[5], ecc_test_public_key1[6]);
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_public_key1[7], ecc_test_public_key1[8],
                                           ecc_test_public_key1[9], ecc_test_public_key1[10], ecc_test_public_key1[11],
                                           ecc_test_public_key1[12], ecc_test_public_key1[13]);

    int res = CRYPTO_ECC_Verify_Key(CRYPTO0_Handler, ecc_test_public_key1);

    CLOGD("ECC224 key verify, result: %d(%s)", res, res==CSK_DRIVER_OK?"Success":"Failure");

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
}


void CRYPTO0_ECC_P512_Generate_Key()
{
    int i = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_BP512R1);


    CRYPTO_ECC_Generate_Key(CRYPTO0_Handler, 0, ecc_test_private_key1, ecc_test_public_key1);

    CLOGD("ECC512 key generated, private key:");

    for(i=0;i<4;i++)
        CLOGD("0x%x,0x%x,0x%x,0x%x", ecc_test_private_key1[i*4+0], ecc_test_private_key1[i*4+1], ecc_test_private_key1[i*4+2],
                                            ecc_test_private_key1[i*4+3]);
    for(i=0;i<8;i++)
        CLOGD("0x%x,0x%x,0x%x,0x%x", ecc_test_public_key1[i*4+0], ecc_test_public_key1[i*4+1], ecc_test_public_key1[i*4+2],
                                           ecc_test_public_key1[i*4+3]);

    int res = CRYPTO_ECC_Verify_Key(CRYPTO0_Handler, ecc_test_public_key1);

    CLOGD("ECC512 key verify, result: %d(%s)", res, res==CSK_DRIVER_OK?"Success":"Failure");

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);

}

static CRYPTO_ECC_CURVE ECC_CURVE_P320 =
{
    40, 1, 0, 0,
    {
        /* p */
            0xD3, 0x5E, 0x47, 0x20, 0x36, 0xBC, 0x4F, 0xB7, 0xE1, 0x3C,
            0x78, 0x5E, 0xD2, 0x01, 0xE0, 0x65, 0xF9, 0x8F, 0xCF, 0xA6,
            0xF6, 0xF4, 0x0D, 0xEF, 0x4F, 0x92, 0xB9, 0xEC, 0x78, 0x93,
            0xEC, 0x28, 0xFC, 0xD4, 0x12, 0xB1, 0xF1, 0xB3, 0x2E, 0x27,
        /* a */
            0x3E, 0xE3, 0x0B, 0x56, 0x8F, 0xBA, 0xB0, 0xF8, 0x83, 0xCC,
            0xEB, 0xD4, 0x6D, 0x3F, 0x3B, 0xB8, 0xA2, 0xA7, 0x35, 0x13,
            0xF5, 0xEB, 0x79, 0xDA, 0x66, 0x19, 0x0E, 0xB0, 0x85, 0xFF,
            0xA9, 0xF4, 0x92, 0xF3, 0x75, 0xA9, 0x7D, 0x86, 0x0E, 0xB4,
        /* b */
            0x52, 0x08, 0x83, 0x94, 0x9D, 0xFD, 0xBC, 0x42, 0xD3, 0xAD,
            0x19, 0x86, 0x40, 0x68, 0x8A, 0x6F, 0xE1, 0x3F, 0x41, 0x34,
            0x95, 0x54, 0xB4, 0x9A, 0xCC, 0x31, 0xDC, 0xCD, 0x88, 0x45,
            0x39, 0x81, 0x6F, 0x5E, 0xB4, 0xAC, 0x8F, 0xB1, 0xF1, 0xA6,
        /* x */
            0x43, 0xBD, 0x7E, 0x9A, 0xFB, 0x53, 0xD8, 0xB8, 0x52,
            0x89, 0xBC, 0xC4, 0x8E, 0xE5, 0xBF, 0xE6, 0xF2, 0x01, 0x37,
            0xD1, 0x0A, 0x08, 0x7E, 0xB6, 0xE7, 0x87, 0x1E, 0x2A, 0x10,
            0xA5, 0x99, 0xC7, 0x10, 0xAF, 0x8D, 0x0D, 0x39, 0xE2, 0x06, 0x11,
        /* y */
            0x14, 0xFD, 0xD0, 0x55, 0x45, 0xEC, 0x1C, 0xC8, 0xAB,
            0x40, 0x93, 0x24, 0x7F, 0x77, 0x27, 0x5E, 0x07, 0x43, 0xFF,
            0xED, 0x11, 0x71, 0x82, 0xEA, 0xA9, 0xC7, 0x78, 0x77, 0xAA,
            0xAC, 0x6A, 0xC7, 0xD3, 0x52, 0x45, 0xD1, 0x69, 0x2E, 0x8E,0xE1,
        /* order */
            0xD3, 0x5E, 0x47, 0x20, 0x36, 0xBC, 0x4F, 0xB7, 0xE1, 0x3C,
            0x78, 0x5E, 0xD2, 0x01, 0xE0, 0x65, 0xF9, 0x8F, 0xCF, 0xA5,
            0xB6, 0x8F, 0x12, 0xA3, 0x2D, 0x48, 0x2E, 0xC7, 0xEE, 0x86,
            0x58, 0xE9, 0x86, 0x91, 0x55, 0x5B, 0x44, 0xC5, 0x93, 0x11,
        /* p', no valid data, only for memory space */
            0xD3, 0x5E, 0x47, 0x20, 0x36, 0xBC, 0x4F, 0xB7, 0xE1, 0x3C,
            0x78, 0x5E, 0xD2, 0x01, 0xE0, 0x65, 0xF9, 0x8F, 0xCF, 0xA5,
            0xB6, 0x8F, 0x12, 0xA3, 0x2D, 0x48, 0x2E, 0xC7, 0xEE, 0x86,
            0x58, 0xE9, 0x86, 0x91, 0x55, 0x5B, 0x44, 0xC5, 0x93, 0x11,
        /* r^2, no valid data, only for memory space */
            0xD3, 0x5E, 0x47, 0x20, 0x36, 0xBC, 0x4F, 0xB7, 0xE1, 0x3C,
            0x78, 0x5E, 0xD2, 0x01, 0xE0, 0x65, 0xF9, 0x8F, 0xCF, 0xA5,
            0xB6, 0x8F, 0x12, 0xA3, 0x2D, 0x48, 0x2E, 0xC7, 0xEE, 0x86,
            0x58, 0xE9, 0x86, 0x91, 0x55, 0x5B, 0x44, 0xC5, 0x93, 0x11,
    }
};



void CRYPTO0_ECC_USER_CURVE_Generate_Key() // 320bit verify fail
{
    int i = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);

    CRYPTO_ECC_Build_Curve(CRYPTO0_Handler, &ECC_CURVE_P320);

    CRYPTO_ECC_Generate_Key(CRYPTO0_Handler, 0, ecc_test_private_key1, ecc_test_public_key1);

    CLOGD("ECC320 key generated, private key:");
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_private_key1[0], ecc_test_private_key1[1], ecc_test_private_key1[2],
                                           ecc_test_private_key1[3], ecc_test_private_key1[4]);
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_private_key1[5], ecc_test_private_key1[6], ecc_test_private_key1[7],
                                           ecc_test_private_key1[8], ecc_test_private_key1[9]);
    CLOGD("public key:");
    for(i=0;i<4;i++)
        CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_public_key1[i*5+0], ecc_test_public_key1[i*5+1], ecc_test_public_key1[i*5+2],
                                           ecc_test_public_key1[i*5+3], ecc_test_public_key1[i*5+4]);


    int res = CRYPTO_ECC_Verify_Key(CRYPTO0_Handler, ecc_test_public_key1);

    CLOGD("ECC320 key verify, result: %d(%s)", res, res==CSK_DRIVER_OK?"Success":"Failure");

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
}

static uint32_t ecc_p256_mult_k []= {
        0xbd88dea7, 0xbf89b123, 0x68aa7b22, 0x5e9816de,
        0xd17857eb, 0xec1d85a3, 0xcb123e2e, 0x27a7824c,
};

static uint32_t ecc_p256_mult_p []= {
         0x294dc56f, 0x3d1ac89e, 0x4080bbd7, 0xb5771b1f,
         0xc150e7b8, 0xe231990b, 0x58a959ef, 0xe6f8832d,
         0x01a8d955, 0x859a54a4, 0xca14746a, 0x8be67b8d,
         0xb3791a7a, 0xb61f278b, 0xa5672cee, 0x8e4a52b5,
};

static uint32_t ecc_p256_mult_result []= {
         0x339f10e6, 0xc8b0f4e4, 0xeb152c4b, 0x54946581,
         0x3b66b782, 0x3a5ec33c, 0x39af089a, 0xae836f30,
         0x0648dc5b, 0x2ce9534f, 0x737ff27d, 0x24944891,
         0x5dd34eb4, 0x107e2e21, 0xd391bc82, 0xfe66f330,
};


void CRYPTO0_ECC_P256_Multiply()
{
    int i = 0;
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);

    systime_1 = SysTick_Value();

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P256);

    CRYPTO_ECC_Multiply(CRYPTO0_Handler, ecc_test_temp_buff1, ecc_p256_mult_p, ecc_p256_mult_k);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("ECC256 ECC multiply finished(use time %dus):", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 16; i++){
        if(ecc_test_temp_buff1[i] == ecc_p256_mult_result[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, ecc_p256_mult_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, ecc_p256_mult_result[i], ecc_test_temp_buff1[i]);
        }
    }
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
}

// brainpoolP160r1
static CRYPTO_ECC_CURVE ECC_CURVE_P160 =
{
    20, 1, 0, 0,
    {
        /* p */
            0xE9, 0x5E, 0x4A, 0x5F, 0x73, 0x70, 0x59, 0xDC, 0x60, 0xDF,
            0xC7, 0xAD, 0x95, 0xB3, 0xD8, 0x13, 0x95, 0x15, 0x62, 0x0F,
        /* a */
            0x34, 0x0E, 0x7B, 0xE2, 0xA2, 0x80, 0xEB, 0x74, 0xE2, 0xBE,
            0x61, 0xBA, 0xDA, 0x74, 0x5D, 0x97, 0xE8, 0xF7, 0xC3, 0x00,
        /* b */
            0x1E, 0x58, 0x9A, 0x85, 0x95, 0x42, 0x34, 0x12, 0x13, 0x4F,
            0xAA, 0x2D, 0xBD, 0xEC, 0x95, 0xC8, 0xD8, 0x67, 0x5E, 0x58,
        /* x */
            0xBE, 0xD5, 0xAF, 0x16, 0xEA, 0x3F, 0x6A, 0x4F, 0x62,
            0x93, 0x8C, 0x46, 0x31, 0xEB, 0x5A, 0xF7, 0xBD, 0xBC, 0xDB, 0xC3,
        /* y */
            0x16, 0x67, 0xCB, 0x47, 0x7A, 0x1A, 0x8E, 0xC3, 0x38,
            0xF9, 0x47, 0x41, 0x66, 0x9C, 0x97, 0x63, 0x16, 0xDA, 0x63, 0x21,
        /* order */
            0xE9, 0x5E, 0x4A, 0x5F, 0x73, 0x70, 0x59, 0xDC, 0x60, 0xDF,
            0x59, 0x91, 0xD4, 0x50, 0x29, 0x40, 0x9E, 0x60, 0xFC, 0x09,
        /* p', no valid data, only for memory space */
            0xa4,0x32,0x51,0x91,0x68,0x28,0x4a,0x60,0x52,0x5f,0x97,0x14,
            0x04,0x52,0x2a,0xb3,0xad,0xbc,0xb3,0x11,
        /* r^2, no valid data, only for memory space */
            0x6c,0xf1,0x2f,0x81,0xc0,0xca,0x7e,0xf8,0xfe,0xd7,0x17,0xe0,
            0xb3,0x33,0xf8,0xd6,0x25,0xbc,0x14,0xff
    }
};

static uint32_t ecc_p160_mult_k []= {
        0xc58e18b9,0xf5ae03e1,0xe080c29c,0xf961139c,0xcd2ff234
};

static uint32_t ecc_p160_mult_p []= {
        0x2fa0f10c,0xe3c352bb,0xcb0f281e,0xaa0c5bbb,0x8b0160c9,0x05302276,0xda68e40d,0x5f441d43,0x6050eb8a,0x00451c7b
};

static uint32_t ecc_p160_mult_result []= {
        0x5644f30f,0xfbe40d59,0x73ad8f37,0xcf0b3489,0x99da5e38,0x544bdc08,0x06490a0a,0xf1ef6176,0x7cfb47fa,0x6ae3cb42
};

// secp128r1
static CRYPTO_ECC_CURVE ECC_CURVE_P128 =
{
    16, 1, 0, 0,
    {
        /* p */
            0xFF, 0xFF, 0xFF, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        /* a */
            0xFF, 0xFF, 0xFF, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
        /* b */
            0xE8, 0x75, 0x79, 0xC1, 0x10, 0x79, 0xF4, 0x3D, 0xD8, 0x24,
            0x99, 0x3C, 0x2C, 0xEE, 0x5E, 0xD3,
        /* x */
            0x16, 0x1F, 0xF7, 0x52, 0x8B, 0x89, 0x9B, 0x2D, 0x0C,
            0x28, 0x60, 0x7C, 0xA5, 0x2C, 0x5B, 0x86, 0xCF,
        /* y */
            0x5A, 0xC8,
            0x39, 0x5B, 0xAF, 0xEB, 0x13, 0xC0, 0x2D, 0xA2, 0x92, 0xDD,
            0xED, 0x7A, 0x83,
        /* order */
            0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x75, 0xA3,
            0x0D, 0x1B, 0x90, 0x38, 0xA1, 0x15,
        /* p', no valid data, only for memory space */
            0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x75, 0xA3,
            0x0D, 0x1B, 0x90, 0x38, 0xA1, 0x15,
        /* r^2, no valid data, only for memory space */
            0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x75, 0xA3,
            0x0D, 0x1B, 0x90, 0x38, 0xA1, 0x15,
    }
};

__attribute__((aligned(32))) static uint8_t ecc_p128_mult_k []= {
        0x0d,0x60,0x10,0x0f,0x5b,0xa0,0xf1,0xf5,0x9f,0xa1,0xde,0xb5,0x7f,0xfe,0xe7,0xc7
};

__attribute__((aligned(32))) static uint8_t ecc_p128_mult_p []= {
        0xfa,0x8e,0x3e,0x9d,0x67,0x3d,0x5f,0x63,0x61,0x84,0xc8,0xe6,0xa6,0xef,0x16,
        0x09,0x5c,0x81,0xb9,0x2a,0x03,0xba,0xda,0xa3,0x28,0x1b,0xde,0xc1,0x27,0x9c,0x9d,0x1d,
};

static uint32_t ecc_p128_mult_result []= {
        0x9321447d,
        0xe673b0bf,
        0x48c51e9a,
        0xea574cd8,
        0xbcea3b65,
        0x4924ca93,
        0x940c181a,
        0x0ffe491b,
};


void CRYPTO0_ECC_USER_CURVE_Multiply() // 160bit & 128bit
{
    int i = 0;
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);

    systime_1 = SysTick_Value();

    // brainpoolP160r1
    CRYPTO_ECC_Build_Curve(CRYPTO0_Handler, &ECC_CURVE_P160);

    CRYPTO_ECC_Multiply(CRYPTO0_Handler, ecc_test_temp_buff1, ecc_p160_mult_p, ecc_p160_mult_k);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("ECC160 ECC multipy finished(use time %dus):", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 10; i++){
        if(ecc_test_temp_buff1[i] == ecc_p160_mult_result[i]){
            CLOGD("I: %d compare success!, data: 0x%08x", i, ecc_p160_mult_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, ecc_p160_mult_result[i], ecc_test_temp_buff1[i]);
        }
    }

    // secp128r1
    systime_1 = SysTick_Value();

    CRYPTO_ECC_Build_Curve(CRYPTO0_Handler, &ECC_CURVE_P128);

    CRYPTO_ECC_Multiply(CRYPTO0_Handler, ecc_test_temp_buff1, (uint32_t*)ecc_p128_mult_p, (uint32_t*)ecc_p128_mult_k);


    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("ECC128 ECC multipy finished(use time %dus):", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 8; i++){
        if(ecc_test_temp_buff1[i] == ecc_p128_mult_result[i]){
            CLOGD("I: %d compare success!, data: 0x%08x", i, ecc_p128_mult_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, ecc_p128_mult_result[i], ecc_test_temp_buff1[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);

}
__attribute__((aligned(32))) static uint8_t ecc_p224_add_p1[] = {
        0x78,0x15,0xb4,0x5c,0x83,0x2d,0x21,0x58,0x88,0xd8,0xca,0x2c,0xb3,0x62,0xae,
        0x14,0x80,0x46,0x87,0x73,0xaa,0x97,0xd9,0x31,0x50,0x2e,0x27,0x7c,0xba,0xe1,0x36,
        0x2f,0xec,0x63,0x2e,0x62,0x6c,0xe2,0x71,0x9c,0x8e,0x11,0xb7,0x77,0x94,0x7f,0xd9,
        0x14,0x2c,0xda,0xf2,0xcf,0x5d,0x2e,0x32,0x74
};

__attribute__((aligned(32))) static uint8_t ecc_p224_add_p2[] = {
        0x2d,0xdb,0x3e,0xc4,0xca,0x64,0xf2,0x40,0x9a,0x26,0x7c,0x0e,0xd5,0x51,0xcf,
        0xa6,0x1d,0x60,0xe6,0x07,0x5e,0x21,0x61,0x45,0xb1,0x9f,0xd9,0xa8,0x35,0x3a,0xb5,
        0x3c,0x02,0xde,0x41,0x2d,0x34,0x3d,0xad,0xea,0xf9,0x22,0x6d,0xd7,0x39,0x75,0xd5,
        0x1f,0x0b,0x7d,0x7b,0x7f,0xf5,0x04,0x88,0xa4
};

static uint32_t ecc_p224_add_result[] = {
        0xecf61996,
        0x31b4a18b,
        0x77fb8a0d,
        0xd9e7ca60,
        0x9db77186,
        0xf9976b0a,
        0x7d1f7508,
        0x67b4ea7c,
        0x1b032173,
        0x27284a5f,
        0xf6d2cc17,
        0xa6012a70,
        0x1b44f171,
        0x7971f540,
};

void CRYPTO0_ECC_P224_ADD()
{
    int i = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P224);

    CRYPTO_ECC_Add(CRYPTO0_Handler, ecc_test_temp_buff1, (uint32_t*)ecc_p224_add_p1, (uint32_t*)ecc_p224_add_p2);

    CLOGD("ECC224 ECC ADD finished:");
    for(i = 0; i < 14; i++){
        if(ecc_test_temp_buff1[i] == ecc_p224_add_result[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, ecc_p224_add_result[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, ecc_p224_add_result[i], ecc_test_temp_buff1[i]);
        }
    }
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
}

void CRYPTO0_ECC_ECDH192_Test()
{

    int i = 0;
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);

    systime_1 = SysTick_Value();

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P192);

    CRYPTO_ECC_Generate_Key(CRYPTO0_Handler, 0, ecc_test_private_key1, ecc_test_public_key1);


    CLOGD("ECC192 key1 generated, private key:");
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_private_key1[0], ecc_test_private_key1[1], ecc_test_private_key1[2],
                                           ecc_test_private_key1[3], ecc_test_private_key1[4], ecc_test_private_key1[5]);
    CLOGD("public key:");
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_public_key1[0], ecc_test_public_key1[1], ecc_test_public_key1[2],
                                           ecc_test_public_key1[3], ecc_test_public_key1[4], ecc_test_public_key1[5]);
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_public_key1[6], ecc_test_public_key1[7], ecc_test_public_key1[8],
                                           ecc_test_public_key1[9], ecc_test_public_key1[10], ecc_test_public_key1[11]);

    CRYPTO_ECC_Generate_Key(CRYPTO0_Handler, 0, ecc_test_private_key2, ecc_test_public_key2);

    CLOGD("ECC192 key2 generated, private key:");
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_private_key2[0], ecc_test_private_key2[1], ecc_test_private_key2[2],
                                           ecc_test_private_key2[3], ecc_test_private_key2[4], ecc_test_private_key2[5]);
    CLOGD("public key:");
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_public_key2[0], ecc_test_public_key2[1], ecc_test_public_key2[2],
                                           ecc_test_public_key2[3], ecc_test_public_key2[4], ecc_test_public_key2[5]);
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_public_key2[6], ecc_test_public_key2[7], ecc_test_public_key2[8],
                                           ecc_test_public_key2[9], ecc_test_public_key2[10], ecc_test_public_key2[11]);


    CRYPTO_ECC_Multiply(CRYPTO0_Handler, ecc_test_temp_buff1, ecc_test_public_key2, ecc_test_private_key1);


    CRYPTO_ECC_Multiply(CRYPTO0_Handler, ecc_test_temp_buff2, ecc_test_public_key1, ecc_test_private_key2);


    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("ECC192 ECDH key generated(use time %dus):", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 12; i++){
        if(ecc_test_temp_buff1[i] == ecc_test_temp_buff2[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, ecc_test_temp_buff1[i]);
        }else{
            CLOGD("I: %d compare failed!, ecdh1 data: 0x%x != ecdh2 data: 0x%x", i, ecc_test_temp_buff1[i], ecc_test_temp_buff2[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
}


void CRYPTO0_ECC_ECDH384_Test()
{

    int i = 0;
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    systime_1 = SysTick_Value();

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P384);

    CRYPTO_ECC_Generate_Key(CRYPTO0_Handler, 0, ecc_test_private_key1, ecc_test_public_key1);


    CLOGD("ECC384 key1 generated, private key:");
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_private_key1[0], ecc_test_private_key1[1], ecc_test_private_key1[2],
                                           ecc_test_private_key1[3], ecc_test_private_key1[4], ecc_test_private_key1[5]);
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_private_key1[6], ecc_test_private_key1[7], ecc_test_private_key1[8],
                                           ecc_test_private_key1[9], ecc_test_private_key1[10], ecc_test_private_key1[11]);
    CLOGD("public key:");
    for(i=0;i<4;i++)
        CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_public_key1[i*6+0], ecc_test_public_key1[i*6+1], ecc_test_public_key1[i*6+2],
                                           ecc_test_public_key1[i*6+3], ecc_test_public_key1[i*6+4], ecc_test_public_key1[i*6+5]);

    CRYPTO_ECC_Generate_Key(CRYPTO0_Handler, 0, ecc_test_private_key2, ecc_test_public_key2);


    CLOGD("ECC384 key2 generated, private key:");
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_private_key2[0], ecc_test_private_key2[1], ecc_test_private_key2[2],
                                           ecc_test_private_key2[3], ecc_test_private_key2[4], ecc_test_private_key2[5]);
    CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_private_key2[6], ecc_test_private_key2[7], ecc_test_private_key2[8],
                                           ecc_test_private_key2[9], ecc_test_private_key2[10], ecc_test_private_key2[11]);
    CLOGD("public key:");
    for(i=0;i<4;i++)
        CLOGD("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", ecc_test_public_key2[i*6+0], ecc_test_public_key2[i*6+1], ecc_test_public_key2[i*6+2],
                                           ecc_test_public_key2[i*6+3], ecc_test_public_key2[i*6+4], ecc_test_public_key2[i*6+5]);


    CRYPTO_ECC_Multiply(CRYPTO0_Handler, ecc_test_temp_buff1, ecc_test_public_key2, ecc_test_private_key1);

    CRYPTO_ECC_Multiply(CRYPTO0_Handler, ecc_test_temp_buff2, ecc_test_public_key1, ecc_test_private_key2);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("ECC384 ECDH key generated(use time %dus):", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 24; i++){
        if(ecc_test_temp_buff1[i] == ecc_test_temp_buff2[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, ecc_test_temp_buff1[i]);
        }else{
            CLOGD("I: %d compare failed!, ecdh1 data: 0x%x != ecdh2 data: 0x%x", i, ecc_test_temp_buff1[i], ecc_test_temp_buff2[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);

}

__attribute__((aligned(32))) static char ecc_ecsda256_test_data[] =
    "To be, or not to be, that is the question,\n"
    "Whether tis nobler in the minde to suffer\n"
    "The slings and arrowes of outragious fortune,\n"
    "Or to take Armes again in a sea of troubles,\n"
;

static uint32_t ecc_ecsda256_sig_digest[] = {
    0x094d9077, 0x0f2bad23, 0xadc28d5d, 0xc7c4cd9c,
    0x08b581da, 0x5e2dac9e, 0xbe63005d, 0x48d8497d,
};

static uint32_t ecc_ecsda256_sig_pub_key[] = {
        0xb67be74f, 0x394254bb, 0x40e55ded, 0xca71d8c8,
        0xd171836d, 0x00652a88, 0x012fc66c, 0x76be4931,
        0x286a677a, 0xb95bc733, 0x6e244524, 0x342f6df0,
        0x6a735306, 0xc19090ff, 0x0d949b6d, 0x65951f0e,
};

static uint32_t ecc_ecsda256_sig_sig_data[] = {
        0xa19f2df8, 0x491f2e97, 0x77a7d447, 0x1521f0d7,
        0x402f9165, 0xf932b63c, 0x98e372f8, 0xcd4929a7,
        0xcdc94911, 0x763636fd, 0xb6271ad1, 0x33f4c8cc,
        0xdd874e29, 0x603f41c7, 0x967fd41a, 0x64030c06,
};

void CRYPTO0_ECC_ECSDA256_Verify_Signature()
{
    int i = 0;
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);

    systime_1 = SysTick_Value();

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_HASH_MODE, CSK_CRYPTO_HASH_SHA256);

    CRYPTO_Hash(CRYPTO0_Handler, (uint32_t*)ecc_ecsda256_test_data, strlen(ecc_ecsda256_test_data), ecc_test_temp_buff1, 0);

    CLOGD("HASH256 finished, digest value check:");
    for(i = 0; i < 8; i++){
        if(ecc_test_temp_buff1[i] == ecc_ecsda256_sig_digest[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, ecc_test_temp_buff1[i]);
        }else{
            CLOGD("I: %d compare failed!, target data: 0x%x != digest data: 0x%x", i, ecc_ecsda256_sig_digest[i], ecc_test_temp_buff1[i]);
        }
    }

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P256);

    //ecc_test_temp_buff1[0] = 1; // test error
    int res = CRYPTO_ECSDA_Verify_Signature(CRYPTO0_Handler, ecc_test_temp_buff1, ecc_ecsda256_sig_pub_key, ecc_ecsda256_sig_sig_data);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;
    CLOGD("ESDA256 sign verify finished, result: %d(%s), use time: %dus", res, res==CSK_DRIVER_OK?"Success":"Failure", systime_diff/CRYPTO_MAIN_FREQ);;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
}

// write efuse and flash before the test
// write efuse: tools/uart_burn_tool/Uart_Burn_Tool.exe -b 115200 -p COM6 -u efuse -f src/drv_demo/drv_demo_arcs/crypto/sec_ecc/test_efuse.bin
// write flash: tools/uart_burn_tool/Uart_Burn_Tool.exe -b 115200 -p COM6 -f src/drv_demo/drv_demo_arcs/crypto/sec_ecc/test_flash.bin -m -d -a 0x0
void CRYPTO0_ECC_ECSDA256_Flash_Verify_Signature()
{
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;
    ls_ota_header_t *hdr = (ls_ota_header_t*)CMN_FLASH_REGION;
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    systime_1 = SysTick_Value();
    int res = CRYPTO_Verify_Flash_Signature(CRYPTO0_Handler, hdr, OTA_SIGN_ECSDA256);
    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);
    CLOGD("Flash verify finished, result: %d(%s), use time: %dus", res, res==CSK_DRIVER_OK?"Success":"Failure", systime_diff/CRYPTO_MAIN_FREQ);

}
