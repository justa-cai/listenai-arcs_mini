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
#include "efuse_ctrl_reg.h"
#include "spiflash.h"
#include "PSRAMManager.h"

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t crypto_aes_user_key[] = {
    0x31313131,
    0x31313131,
    0x31313131,
    0x31313131,
    0x6d6e6f70,
    0x696a6b6c,
    0x65666768,
    0x61626364,
};

// CBC initialize vector
static uint32_t encrypto_aes_cbc_iv[4] = {
    0x6d6e6f70,
    0x696a6b6c,
    0x65666768,
    0x61626364,
};

static uint32_t encrypto_aes_source_data_32w[32] = {
    0x61616161,
    0x61616161,
    0x61616161,
    0x61616161,
    0x62626262,
    0x62626262,
    0x62626262,
    0x62626262,
    0x63636363,
    0x63636363,
    0x63636363,
    0x63636363,
    0x64646464,
    0x64646464,
    0x64646464,
    0x64646464,
    0x65656565,
    0x65656565,
    0x65656565,
    0x65656565,
    0x66666666,
    0x66666666,
    0x66666666,
    0x66666666,
    0x67676767,
    0x67676767,
    0x67676767,
    0x67676767,
    0x68686868,
    0x68686868,
    0x68686868,
    0x68686868,
};

__attribute__((aligned(32))) uint32_t encrypto_aes_result_data_32w[200] = {0};
//uint32_t *encrypto_aes_result_data_32w = (uint32_t*)0x20050000;

// ecf8427e2b7e151628aed2a6abf7158809cf4f3cd41d8cd98f00b204e9800998
uint32_t crypto_aes256_user_key[] =
{
//        0xe9800998,
//        0x8f00b204,
//        0xd41d8cd9,
//        0x09cf4f3c,
//        0xabf71588,
//        0x28aed2a6,
//        0x2b7e1516,
//        0xecf8427e,
        0x7e42f8ec,
        0x16157e2b,
        0xa6d2ae28,
        0x8815f7ab,
//        0x7e42f8ec,
//        0x16157e2b,
//        0xa6d2ae28,
//        0x8815f7ab,
        0x3c4fcf09,
        0xd98c1dd4,
        0x04b2008f,
        0x980980e9,
};


/* Encrypt AES ECB*/
static uint32_t encrypto_aes_ecb_destination_data_32w[32] = {
    0x02d1b9c6,
    0x69b1eadd,
    0xdbcb24f3,
    0x0af033e7,
    0xd01e9e16,
    0xc64033fa,
    0xdf644935,
    0x3bf33ae2,
    0xc4c7b2b1,
    0x1657d1b3,
    0x8b886315,
    0xee810ee4,
    0x33d62a1c,
    0xbf97e0b7,
    0x196674ec,
    0xd60063a9,
    0xaf0cdb08,
    0x75e177a1,
    0x7519a51b,
    0x1b688c7a,
    0xa6653ae4,
    0x793f94c5,
    0x6140deb0,
    0xa2c0898f,
    0x82896e09,
    0xde36a161,
    0x36118c12,
    0x37176680,
    0xf6b1f404,
    0x52009763,
    0x45ddf6b0,
    0x890614c6,
};


void CRYPTO0_AES128_ECB_Encrypt_User_Key(){
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_128, 0);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes_user_key);

    systime_1 = SysTick_Value();

    CRYPTO_ECB_Encrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes_source_data_32w, (32*4), (uint32_t *)encrypto_aes_result_data_32w);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    int i = 0;

    for(i = 0; i < 32; i++){
        if(encrypto_aes_ecb_destination_data_32w[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_ecb_destination_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_ecb_destination_data_32w[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("AES ECB 128 encryption test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}



void CRYPTO0_AES128_ECB_Decrypt_User_Key(){

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_128, 0);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes_user_key);

    CRYPTO_ECB_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes_ecb_destination_data_32w, (32*4), (uint32_t *)encrypto_aes_result_data_32w);

    int i = 0;

    for(i = 0; i < 32; i++){
        if(encrypto_aes_source_data_32w[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_source_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_source_data_32w[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

// write aes key to efuse
void CRYPTO0_AES_Write_Efuse_Key()
{
    extern void crypto_efuse_write_word(int index, uint32_t val);
    extern void efuse_force_auto_load();
#define __HAL_EFUSE_CLK_ENABLE()    \
do { \
    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.AON_SEL_EFUSE_CLK = 0x1; \
    IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_EFUSE_CLK = 0x1; \
} while(0)

#define __HAL_EFUSE_CLK_DISABLE()    \
do { \
    IIP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_EFUSE_CLK = 0x0; \
} while(0)

#define __HAL_EFUSE_POWER_ENABLE()    \
do { \
    IP_AON_CTRL->REG_AON_TUNE2.bit.EN_PSW_EFUSE = 0x1; \
} while(0)

#define __HAL_EFUSE_POWER_DISABLE()    \
do { \
    IP_AON_CTRL->REG_AON_TUNE2.bit.EN_PSW_EFUSE = 0x0; \
} while(0)

    int i;

    // use pclk for efuse
    // enable efuse
    __HAL_EFUSE_CLK_ENABLE();

    // enable power for efuse program
    __HAL_EFUSE_POWER_ENABLE();

    for(i=0; i<8; i++)
    {
        crypto_efuse_write_word(32+i, crypto_aes256_user_key[i]);
        crypto_efuse_write_word(120+i, crypto_aes256_user_key[i]);
    }

    // swd protect mode: 0-open, 1-key, 2-wave, 3-close
    crypto_efuse_write_word(19, 0);
    // swd key: 0x17158809cf4bccdd
    crypto_efuse_write_word(22, 0xcf4bccdd);
    crypto_efuse_write_word(23, 0x17158809);
    // write efuse valid flag
    crypto_efuse_write_word(0, 1);

    efuse_force_auto_load();

    // disable power for efuse program
    __HAL_EFUSE_POWER_DISABLE();
}

static const uint32_t crypto_flash_enc_block2[] ={
        0x00df007d, 0xb00020ff, 0x0020ff06, 0x7b0020ff,
        0x00df0075, 0xb10020ff, 0x0020ff16, 0x730020ff,
        0x00df006d, 0xb20020ff, 0x0020ff26, 0x6b0020ff,
        0x00df0065, 0xb30020ff, 0x0020ff36, 0x630020ff,
        0x00df005d, 0xb40020ff, 0x0020ff46, 0x5b0020ff,
        0x00df0055, 0xb50020ff, 0x0020ff56, 0x530020ff,
        0x00df004d, 0xb60020ff, 0x0020ff66, 0x4b0020ff,
        0x00df0045, 0xb70020ff, 0x0020ff76, 0x430020ff,
        0x00df003d, 0xb80020ff, 0x0020ff86, 0x3b0020ff,
        0x00df0035, 0xb90020ff, 0x0020ff96, 0x330020ff,
        0x00df002d, 0xba0020ff, 0x0020ffa6, 0x2b0020ff,
        0x00df0025, 0xbb0020ff, 0x0020ffb6, 0x230020ff,
        0x00df001d, 0xbc0020ff, 0x0020ffc6, 0x1b0020ff,
        0x00df0015, 0xbd0020ff, 0x0020ffd6, 0x130020ff,
        0x00df000d, 0xbe0020ff, 0x0020ffe6, 0x0b0020ff,
        0x00df0005, 0xbf0020ff, 0x0020fff6, 0x030020ff,
};

static const uint32_t crypto_flash_enc_block3[] ={
        0x00cf007d, 0xb00030ff, 0x0030ff06, 0x7b0030ff,
        0x00cf0075, 0xb10030ff, 0x0030ff16, 0x730030ff,
        0x00cf006d, 0xb20030ff, 0x0030ff26, 0x6b0030ff,
        0x00cf0065, 0xb30030ff, 0x0030ff36, 0x630030ff,
        0x00cf005d, 0xb40030ff, 0x0030ff46, 0x5b0030ff,
        0x00cf0055, 0xb50030ff, 0x0030ff56, 0x530030ff,
        0x00cf004d, 0xb60030ff, 0x0030ff66, 0x4b0030ff,
        0x00cf0045, 0xb70030ff, 0x0030ff76, 0x430030ff,
        0x00cf003d, 0xb80030ff, 0x0030ff86, 0x3b0030ff,
        0x00cf0035, 0xb90030ff, 0x0030ff96, 0x330030ff,
        0x00cf002d, 0xba0030ff, 0x0030ffa6, 0x2b0030ff,
        0x00cf0025, 0xbb0030ff, 0x0030ffb6, 0x230030ff,
        0x00cf001d, 0xbc0030ff, 0x0030ffc6, 0x1b0030ff,
        0x00cf0015, 0xbd0030ff, 0x0030ffd6, 0x130030ff,
        0x00cf000d, 0xbe0030ff, 0x0030ffe6, 0x0b0030ff,
        0x00cf0005, 0xbf0030ff, 0x0030fff6, 0x030030ff,
};

static int crypto_check_flash_enc(uint8_t* buff, uint32_t len)
{
    int i = 0;
    int count = 0;
    int n, res = 0;
    uint32_t chk;

    while (i < len)
    {
        n = ((~((buff[i + 0] & buff[i + 15]) >> 3))&0xf) | ((buff[i + 8]>>4) & buff[i + 7]);
        if (n != count)
            res ++;

        chk = ((buff[i + 7] ^ (buff[i + 8]>>4)) | (buff[i + 8] ^ buff[i + 7]<<4))&0xff;
        chk += (((buff[i + 12] | buff[i + 1]) ^ (buff[i + 11] & buff[i + 6]))&0xff)<<8;
        chk += ((((buff[i + 13] & buff[i + 10]) | buff[i + 5]) ^ buff[i + 2])&0xff)<<16;
        chk += ((buff[i + 3] ^ ((buff[i + 4] & buff[i + 9]) | buff[i + 14]))&0xff)<<24;
        buff[i] = ((buff[i + 7] ^ (buff[i + 8]>>4)) | (buff[i + 8] ^ buff[i + 7]<<4))&0xff;
        buff[i+1] = (((buff[i + 12] | buff[i + 1]) ^ (buff[i + 11] & buff[i + 6]))&0xff);
        buff[i+2] = ((((buff[i + 13] & buff[i + 10]) | buff[i + 5]) ^ buff[i + 2])&0xff);
        buff[i+3] = ((buff[i + 3] ^ ((buff[i + 4] & buff[i + 9]) | buff[i + 14]))&0xff);

        if (chk != 0xffffffb6)
            res ++;

        count = (count + 1) & 0xf;
        i += 16;
    };

    return res;
}

#define CRYPTO_FLASH_ENC_BLOCK_SIZE      (0x1000)
static uint32_t crypto_flash_buff[CRYPTO_FLASH_ENC_BLOCK_SIZE/4];
FLASH_DEV crypto_flash = {
        .base_addr = CMN_FLASHC_BASE,
        .d_width = 1,
//      .sclk_div = 0,    //divider is 2
//        .sclk_div = 0xFF, //divider is 1
      .sclk_div = 1,    //divider is 4
        .run_mod = RUN_WITH_INT,//RUN_WITH_INT//RUN_WITHOUT_INT
        .timeout = 2000000,
        .addr_bytes = 3,
        .addr_auto = 0,
};

void CRYPTO0_AES128_Decrypt_Flash_Test()
{
    /*
     * Use four block to test, each block 4KB, with same data, with or without encryption
     *
     * Flash bin: block0, block2: raw data, block1: encdata, block3: blank
     * eFuse config: enc address scope: block1 ~ block3
     * test prepare:
     * 1. program flash
     *     tools/uart_burn_tool/Uart_Burn_Tool -b 115200 -p COM6 -f src/drv_demo/drv_demo_arcs/crypto/test_flash_enc.bin -m -d -a 0x0
     */
    int i, err_count, result;
    uint32_t *buff_a = (uint32_t*)0x08000000;
    uint32_t *buff_b = (uint32_t*)0x10000000;
    uint32_t *buff_c = (uint32_t*)0x18000000;
    uint32_t *buff_d = (uint32_t*)0x1c000000;
    CMN_SYSCFG_RegDef *g_sysctrl = IP_SYSCTRL;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_128, 0);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_EFUSE2, 0);

    // 1. config block 0 to region b and enable enc
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_TGT_SLV_SEL = 1;
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_A = 0;
    g_sysctrl->REG_CIPHER_CTRL2.bit.CIPHER_DEV_OFFSET_REGION_B = 0;
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_B = 1;
    for(i = 0, err_count=0; i < CRYPTO_FLASH_ENC_BLOCK_SIZE/4; i++){
        if(buff_a[i] == buff_b[i]){
            if((i&0xff)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, buff_a[i]);
        }else{
            err_count ++;
            if(err_count < 20)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, buff_a[i], buff_b[i]);
        }
    }
    CLOGD("I: Flash Block0 check result: error/total words: %d/%d(should all error)", err_count, CRYPTO_FLASH_ENC_BLOCK_SIZE/4);

    // check2: block0 is same as block1 after decrypt
    // auto load efuse
    CLOGD("Check flash decrypt with block1:");
    g_sysctrl->REG_CIPHER_CTRL2.bit.CIPHER_DEV_OFFSET_REGION_B = 1;
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_B = 1;
    for(i = 0, err_count=0; i < CRYPTO_FLASH_ENC_BLOCK_SIZE/4; i++){
        if(buff_a[i] == buff_b[i]){
            if((i&0xff)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, buff_a[i]);
        }else{
            err_count ++;
            if(err_count < 20)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, buff_a[i], buff_b[i]);
        }
    }
    CLOGD("I: Flash Block1 check result: error/total words: %d/%d", err_count, CRYPTO_FLASH_ENC_BLOCK_SIZE/4);


    // check3: decrypt block2 check result
    CLOGD("Check flash decrypt with block2:");
    //    config block2 to region d and disable enc
    g_sysctrl->REG_CIPHER_CTRL1.bit.CIPHER_DEV_OFFSET_REGION_D = 2;
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_D = 0;

    //    decrypt region d(block2), check result
    for(i = 0; i < CRYPTO_FLASH_ENC_BLOCK_SIZE/4; i++)
        crypto_flash_buff[i] = buff_a[i]^buff_d[i];

    CRYPTO_ECB_Decrypt(CRYPTO0_Handler, (uint32_t *)crypto_flash_buff, CRYPTO_FLASH_ENC_BLOCK_SIZE, (uint32_t *)crypto_flash_buff);

    for(i = 0, err_count=0; i < sizeof(crypto_flash_enc_block2)/4; i++){
        if(crypto_flash_buff[i] == crypto_flash_enc_block2[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, crypto_flash_buff[i]);
        }else{
            err_count ++;
            if(err_count < 20)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, crypto_flash_enc_block2[i], crypto_flash_buff[i]);
        }
    }
    err_count += crypto_check_flash_enc((uint8_t*)crypto_flash_buff, CRYPTO_FLASH_ENC_BLOCK_SIZE);
    CLOGD("I: Block2 decrypt check result: error count: %d/%d", err_count, CRYPTO_FLASH_ENC_BLOCK_SIZE/4);

    // check4: write block0 to block3, disable flash encrypt and decrypt block3, check with block0
    memcpy(crypto_flash_buff, (uint32_t*)buff_a, CRYPTO_FLASH_ENC_BLOCK_SIZE);
    // enable encryption
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_A = 1;

    /// initialize flash driver
    flash_init(&crypto_flash, 0, 0);
    flash_write_protection_set(&crypto_flash, false);

    /// earse flash
    result = flash_erase(&crypto_flash, 3*CRYPTO_FLASH_ENC_BLOCK_SIZE, CRYPTO_FLASH_ENC_BLOCK_SIZE);

    result = flash_write(&crypto_flash, 3*CRYPTO_FLASH_ENC_BLOCK_SIZE, crypto_flash_buff, CRYPTO_FLASH_ENC_BLOCK_SIZE);

    flash_write_protection_set(&crypto_flash, true);

    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_A = 0;
    g_sysctrl->REG_CIPHER_CTRL1.bit.CIPHER_DEV_OFFSET_REGION_D = 3;
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_D = 1;

    CLOGD("I: Block3 write finish, compare result:");
    for(i = 0, err_count=0; i < CRYPTO_FLASH_ENC_BLOCK_SIZE/4; i++){
        if(buff_a[i] == buff_d[i]){
            if((i&0xff)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, buff_a[i]);
        }else{
            err_count ++;
            if(err_count < 20)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, buff_a[i], buff_d[i]);
        }
    }
    CLOGD("I: Block3 compare result: error count: %d/%d", err_count, CRYPTO_FLASH_ENC_BLOCK_SIZE/4);

    CLOGD("Check flash encrypt with block3:");
    for(i = 0; i < CRYPTO_FLASH_ENC_BLOCK_SIZE/4; i++)
        crypto_flash_buff[i] = buff_a[i]^buff_a[i+3*CRYPTO_FLASH_ENC_BLOCK_SIZE/4];

    CRYPTO_ECB_Decrypt(CRYPTO0_Handler, (uint32_t *)crypto_flash_buff, CRYPTO_FLASH_ENC_BLOCK_SIZE, (uint32_t *)crypto_flash_buff);

    for(i = 0, err_count=0; i < sizeof(crypto_flash_enc_block3)/4; i++){
        if(crypto_flash_buff[i] == crypto_flash_enc_block3[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, crypto_flash_buff[i]);
        }else{
            err_count ++;
            if(err_count < 20)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, crypto_flash_enc_block3[i], crypto_flash_buff[i]);
        }
    }
    err_count += crypto_check_flash_enc((uint8_t*)crypto_flash_buff, CRYPTO_FLASH_ENC_BLOCK_SIZE);
    CLOGD("I: Block3 decrypt check result: error count: %d/%d", err_count, CRYPTO_FLASH_ENC_BLOCK_SIZE/16+sizeof(crypto_flash_enc_block3)/4);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

void CRYPTO0_AES128_Decrypt_PSRAM_Test()
{
    /*
     * Use four block to test, each block 4KB, with same data, with or without encryption
     */
    int i, err_count;
    uint32_t *buff_a = (uint32_t*)0x08000000;
    uint32_t *buff_b = (uint32_t*)0x10000000;
    uint32_t *buff_c = (uint32_t*)0x18000000;
    uint32_t *buff_d = (uint32_t*)0x1c000000;
    CMN_SYSCFG_RegDef *g_sysctrl = IP_SYSCTRL;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_128, 0);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_EFUSE2, 0);

    PSRAM_Initialize(0, 0, 1);

//    // byte operate test
//    uint8_t *buff = (uint8_t *)0x08000000;
//    for(i=0; i<256; i++)
//        buff[i] = i;
//    CLOGD("I: PSRAM encrypt check");
//    for(i=0; i<256; i++)
//        buff[255-i] = i;
//    CLOGD("I: PSRAM encrypt check");

    uint32_t systime_1 = SysTick_Value();
    srand(systime_1);
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_TGT_SLV_SEL = 0;
    // 1. generate random 4K data to block 0
    for(i=0; i<CRYPTO_FLASH_ENC_BLOCK_SIZE/4; i++)
        buff_a[i] = random();

    // 2. config block 1 to region b and enable enc
    g_sysctrl->REG_CIPHER_CTRL2.bit.CIPHER_DEV_OFFSET_REGION_B = 1;
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_B = 1;
    //    copy region a(block 0) to  region b(block 1)
    memcpy(buff_b, buff_a, CRYPTO_FLASH_ENC_BLOCK_SIZE);
    //    disable region b enc
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_B = 0;
    //    compare a(block 0) to  region b(block 1), should all error
    for(i = 0, err_count=0; i < CRYPTO_FLASH_ENC_BLOCK_SIZE/4; i++){
        if(buff_a[i] == buff_b[i]){
            if((i&0xff)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, buff_a[i]);
        }else{
            err_count ++;
            if(err_count < 20)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, buff_a[i], buff_b[i]);
        }
    }
    CLOGD("I: PSRAM encrypt check 1 result: error count: %d/%d(should all error)", err_count, CRYPTO_FLASH_ENC_BLOCK_SIZE/4);

    //    copy region b(block 1) to region b(block2)
    memcpy(buff_b+CRYPTO_FLASH_ENC_BLOCK_SIZE/4, buff_b, CRYPTO_FLASH_ENC_BLOCK_SIZE);
    //    enable region b enc
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_B = 1;
    //    copy region a(block 2) to  region b(block 1)
    memcpy(buff_b, buff_a+2*CRYPTO_FLASH_ENC_BLOCK_SIZE/4, CRYPTO_FLASH_ENC_BLOCK_SIZE);
    //    compare a(block 0) to  region a(block 1)
    for(i = 0, err_count=0; i < CRYPTO_FLASH_ENC_BLOCK_SIZE/4; i++){
        if(buff_a[i] == buff_a[i+CRYPTO_FLASH_ENC_BLOCK_SIZE/4]){
            if((i&0xff)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, buff_a[i]);
        }else{
            err_count ++;
            if(err_count < 20)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, buff_a[i], buff_a[i+CRYPTO_FLASH_ENC_BLOCK_SIZE/4]);
        }
    }
    CLOGD("I: PSRAM encrypt check 2 result: error count: %d/%d", err_count, CRYPTO_FLASH_ENC_BLOCK_SIZE/4);

    // 3. encrypt block 0 data and save result to block3
    CRYPTO_ECB_Encrypt(CRYPTO0_Handler, (uint32_t *)crypto_flash_enc_block3, sizeof(crypto_flash_enc_block3), (uint32_t *)encrypto_aes_result_data_32w);

    for(i=0;i<sizeof(crypto_flash_enc_block3)/4;i++)
        buff_a[i+3*CRYPTO_FLASH_ENC_BLOCK_SIZE/4] = buff_a[i]^encrypto_aes_result_data_32w[i];

    //    config block 3 to region c and enable enc
    g_sysctrl->REG_CIPHER_CTRL2.bit.CIPHER_DEV_OFFSET_REGION_C = 3;
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_C = 1;
    //    compare region c(block 3) with region a(block 0)
    for(i = 0, err_count=0; i < sizeof(crypto_flash_enc_block3)/4; i++){
        if(buff_a[i] == buff_c[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, buff_a[i]);
        }else{
            err_count ++;
            if(err_count < 20)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, buff_a[i], buff_c[i]);
        }
    }
    CLOGD("I: PSRAM encrypt check 3 result: error count: %d/%d", err_count, sizeof(crypto_flash_enc_block3)/4);

    // 4. config block2 to region c and enable enc
    g_sysctrl->REG_CIPHER_CTRL2.bit.CIPHER_DEV_OFFSET_REGION_C = 2;
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_C = 1;
    //    copy data from region a(block 0) to region c(block2)
    memcpy(buff_c, buff_a, CRYPTO_FLASH_ENC_BLOCK_SIZE);
    //    config block2 to region d and disable enc
    g_sysctrl->REG_CIPHER_CTRL1.bit.CIPHER_DEV_OFFSET_REGION_D = 2;
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_D = 0;
    //    decrypt region d(block2), check result
    for(i = 0; i < CRYPTO_FLASH_ENC_BLOCK_SIZE/4; i++)
        buff_d[i] ^= buff_a[i];

    CRYPTO_ECB_Decrypt(CRYPTO0_Handler, (uint32_t *)buff_d, CRYPTO_FLASH_ENC_BLOCK_SIZE, (uint32_t *)buff_a);

    for(i = 0, err_count=0; i < sizeof(crypto_flash_enc_block2)/4; i++){
        if(buff_a[i] == crypto_flash_enc_block2[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%08x", i, buff_a[i]);
        }else{
            err_count ++;
            if(err_count < 20)
                CLOGD("I: %d compare failed!, destination data: 0x%08x != result data: 0x%08x", i, crypto_flash_enc_block2[i], buff_a[i]);
        }
    }
    err_count += crypto_check_flash_enc((uint8_t*)buff_a, CRYPTO_FLASH_ENC_BLOCK_SIZE);
    CLOGD("I: PSRAM decrypt check 4 result: error count: %d/%d", err_count, CRYPTO_FLASH_ENC_BLOCK_SIZE/16+sizeof(crypto_flash_enc_block2)/4);


    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

void CRYPTO0_PSRAM_Cihper_Region_Validation(void){
    uint32_t value = 0;
    uint32_t i = 0, j = 0;
    uint32_t total;
    uint32_t offset_b, offset_c, offset_d;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    CLOGD("PSRAM CIPHER Region Validation");

    uint32_t *region_a_p = (uint32_t*)0x08000000;
    uint32_t *region_b_p = (uint32_t*)0x10000000;
    uint32_t *region_c_p = (uint32_t*)0x18000000;
    uint32_t *region_d_p = (uint32_t*)0x1c000000;

    PSRAM_Initialize(0, 0, 1);

    IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_TGT_SLV_SEL = 0;

    IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_A = 0x0;
    IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_B = 0x0;
    IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_C = 0x0;
    IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_D = 0x0;

    // total block with 4KB size
    total = 8*1024*1024/(4*1024);

    // initialize all the psram memory
    for (i = 0; i < total*1024; i++){
        *(region_a_p + i) = i*i;
    }

    for(offset_b = 0; offset_b<total; offset_b++)
    {
        offset_c = (offset_b*3) % total;
        offset_d = (offset_b*7) % total;
        CLOGD("CIPHER Region Validation with offset B = %d, offset C = %d, offset D = %d", offset_b, offset_c, offset_d);

        IP_SYSCTRL->REG_CIPHER_CTRL2.bit.CIPHER_DEV_OFFSET_REGION_B = offset_b;
        IP_SYSCTRL->REG_CIPHER_CTRL2.bit.CIPHER_DEV_OFFSET_REGION_C = offset_c;
        IP_SYSCTRL->REG_CIPHER_CTRL1.bit.CIPHER_DEV_OFFSET_REGION_D = offset_d;

        for (i = 0, j = offset_b*1024; j < total*1024; i++, j++){
            if (*(region_a_p + j) != *(region_b_p + i)){
                CLOGE("Region A(%d) != Region B(%d)", j, i);
                return;
            }
        }

        //CLOGD("REGION A == REGION B");

        for (i = 0, j = offset_c*1024; j < total*1024; i++, j++){
            if (*(region_a_p + j) != *(region_c_p + i)){
                CLOGE("Region A(%d) != Region C(%d)", j, i);
            }
        }

        //CLOGD("REGION A == REGION C");

        for (i = 0, j = offset_d*1024; j < total*1024; i++, j++){
            if (*(region_a_p + j) != *(region_d_p + i)){
                CLOGE("Region A(%d) != Region D(%d)", j, i);
            }
        }

        //CLOGD("REGION A == REGION D");

//        for (i = 0, j = REGION_SPE_SIZE/4; i < 2 * REGION_SPE_SIZE/4; i++, j++){
//            if (*(region_b_p + j) != *(region_c_p + i)){
//                CLOGE("Region B(%d) != Region C(%d)", j, i);
//                return;
//            }
//        }
//
//        //CLOGD("REGION B == REGION C");
//
//        for (i = 0, j = REGION_SPE_SIZE/4; i < REGION_SPE_SIZE/4; i++, j++){
//            if (*(region_c_p + j) != *(region_d_p + i)){
//                CLOGE("Region C(%d) != Region D(%d)", j, i);
//                return;
//            }
//        }

        //CLOGD("REGION C == REGION D");

    }
}

uint32_t crypto_aes256_iv[] = {
        0x578a2dae,
        0x9cac031e,
        0xac6fb79e,
        0x518eaf45,
};

uint32_t crypto_aes256_count[] = {
        0x45af8e51,
        0x9eb76fac,
        0x1e03ac9c,
        0xae2d8a57,
};


uint32_t encrypto_aes256_raw_data[] = {
        0xe2bec16b, // ccm aad
        0x969f402e,
        0x117e3de9,// ccm raw data
        0x2a179373,
        0x578a2dae,
        0x9cac031e,
        0xac6fb79e,
        0x518eaf45,
        0x461cc830,
        0x11e45ca3,
        0x19c1fbe5,
        0xef520a1a,
        0x45249ff6,
        0x179b4fdf,
        0x7b412bad,
        0x10376ce6,

};

uint32_t encrypto_aes256_enc_data[] = {
        0x67724ead,
        0x3f1c035b,
        0xaf06084a,
        0x8c1b037e,
        0x6758920b,
        0x54e9ab4a,
        0xdda91bdb,
        0xbba4550f,
        0x0ad15807,
        0x2ae77e36,
        0xbd96605a,
        0xc61e8e2b,
        0xdea06d60,
        0x806f2031,
        0xe629b877,
        0x09b0e27c,

};

void CRYPTO0_AES256_CBC_Encrypt_User_Key()
{
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;
    int i = 0;
    uint32_t aes_length[4] = {0,0,0,64};

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    systime_1 = SysTick_Value();

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_CBC);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_256, 0);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes256_user_key);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)crypto_aes256_iv);

    CRYPTO_CBC_Encrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_raw_data, (16*4), (uint32_t *)encrypto_aes_result_data_32w);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;

    CLOGD("AES256 CBC encrypt test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);
    for(i = 0; i < 16; i++){
        if(encrypto_aes256_enc_data[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes256_enc_data[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes256_enc_data[i], encrypto_aes_result_data_32w[i]);
        }
    }

    systime_1 = SysTick_Value();
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)crypto_aes256_iv);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    CRYPTO_AES_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_enc_data, (16), (uint32_t *)encrypto_aes_result_data_32w);
    CRYPTO_AES_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_enc_data+4, (16), (uint32_t *)encrypto_aes_result_data_32w+4);
    CRYPTO_AES_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_enc_data+8, (32), (uint32_t *)encrypto_aes_result_data_32w+8);

    systime_2 = SysTick_Value();

    systime_diff = systime_2 - systime_1;
    CLOGD("AES256 CBC Decrypt test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 15; i++){
        if(encrypto_aes256_raw_data[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes256_raw_data[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes256_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

void CRYPTO0_AES256_CBC_Encrypt_Efuse2_Key()
{
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_256, 0);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_EFUSE2, 0); // crypto_aes256_user_key
    //CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes256_user_key);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)crypto_aes256_iv);
    CRYPTO_CBC_Encrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_raw_data, (16*4), (uint32_t *)encrypto_aes_result_data_32w);

    int i = 0;

    for(i = 0; i < 16; i++){
        if(encrypto_aes256_enc_data[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes256_enc_data[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes256_enc_data[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)crypto_aes256_iv);
    CRYPTO_CBC_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_enc_data, (16*4), (uint32_t *)encrypto_aes_result_data_32w);

    for(i = 0; i < 16; i++){
        if(encrypto_aes256_raw_data[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes256_raw_data[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes256_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}


static uint32_t encrypto_aes_source_data_16w[16] = {
    0x61616161,    0x61616161,    0x61616161,    0x61616161,
    0x62626262,    0x62626262,    0x62626262,    0x62626262,
    0x63636363,    0x63636363,    0x63636363,    0x63636363,
    0x64646464,    0x64646464,    0x64646464,    0x64646464,
};

static uint32_t encrypto_aes_ecb_destination_data_16w[16] = {
    0x02D1B9C6,    0x69B1EADD,    0xDBCB24F3,    0x0AF033E7,
    0xD01E9E16,    0xC64033FA,    0xDF644935,    0x3BF33AE2,
    0xC4C7B2B1,    0x1657D1B3,    0x8B886315,    0xEE810EE4,
    0x33D62A1C,    0xBF97E0B7,    0x196674EC,    0xD60063A9,
};

static uint32_t encrypto_aes_ecb192_destination_data_16w[16] = {
    0x86643f84,    0xf0648160,    0xc561dae9,    0x98370231,
    0xe6532273,    0xa14cf9cc,    0x7f330625,    0x6f234c15,
    0xd1d77069,    0xd0195d68,    0x7d56651d,    0x4daf0a67,
    0xd37a1443,    0x41a7ea99,    0x03ee87b7,    0x934026b1,

};

/*Encrypt AEC CBC 192 */
static uint32_t encrypto_aes_cbc_destination_data_32w[32] = {
    0x6235608e,    0x1e2b30f0,    0xa2455b16,    0xd5e6f05d,
    0xd9fe3195,    0x38d81bdd,    0xc66d2cb5,    0x11f9a3b6,
    0x9773357e,    0x1722e35e,    0x0f641e15,    0x3c197c8e,
    0x710368a2,    0x4f99300f,    0x229810b5,    0x967eb609,
    0xffb168a1,    0x5247753e,    0x253bf965,    0x8dcc9451,
    0x74caecd8,    0xb488591c,    0x933bc500,    0x573bcb55,
    0x56b337b9,    0x6942f6a4,    0xeaf31e88,    0x10fbc381,
    0x95513add,    0x0e3f5e30,    0xefe87c3b,    0xb3fff08f,

};


void CRYPTO0_AES192_ECB_Encrypt_User_Key(){
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_192, 0);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes_user_key);

    systime_1 = SysTick_Value();

    CRYPTO_ECB_Encrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes_source_data_16w, (16*4), (uint32_t *)encrypto_aes_result_data_32w);

    systime_2 = SysTick_Value();

    int i = 0;

    for(i = 0; i < 16; i++){
        if(encrypto_aes_ecb192_destination_data_16w[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_ecb_destination_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_ecb192_destination_data_16w[i], encrypto_aes_result_data_32w[i]);
        }
    }

    systime_diff = systime_2 - systime_1;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

void CRYPTO0_AES192_CBC_Encrypt_User_Key(){

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_KEY_SIZE_192, 0);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes_user_key);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)encrypto_aes_cbc_iv);

    CRYPTO_CBC_Encrypt(CRYPTO0_Handler,  (uint32_t *)encrypto_aes_source_data_32w,\
            (32*4), (uint32_t *)encrypto_aes_result_data_32w);

    int i = 0;

    for(i = 0; i < 32; i++){
        if(encrypto_aes_cbc_destination_data_32w[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_cbc_destination_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_cbc_destination_data_32w[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}


void CRYPTO0_AES192_CBC_Decrypt_User_Key(){

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_192, 0);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes_user_key);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)encrypto_aes_cbc_iv);

    CRYPTO_CBC_Decrypt(CRYPTO0_Handler,  (uint32_t *)encrypto_aes_cbc_destination_data_32w,\
            (32*4), (uint32_t *)encrypto_aes_result_data_32w);

    int i = 0;

    for(i = 0; i < 32; i++){
        if(encrypto_aes_source_data_32w[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_source_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_source_data_32w[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

static uint32_t encrypto_aes_ctr_destination_data_32w[16] = {
    0xc8dbd96b,    0x8b061451,    0xa4c8f2c5,    0xb2bb5e76,
    0xc2df3b62,    0x0fc0e394,    0x982c3044,    0x14822844,
    0x6edfdd6b,    0x8beae3a6,    0x0f850632,    0xcc52f482,
    0xbf0c8c6e,    0xdf8b194e,    0xc83c9705,    0x5031b35e,

};

void CRYPTO0_AES192_CTR_Encrypt_User_Key()
{
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint32_t aes_length[4] = {16, 16, 0, 16*4};

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_192, 0);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_CTR);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes256_user_key);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_IV, (uint32_t)crypto_aes256_iv);

    CRYPTO_AES_Encrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_raw_data, (16*4), (uint32_t *)encrypto_aes_result_data_32w);

    int i = 0;

    for(i = 0; i < 16; i++){
        if(encrypto_aes_ctr_destination_data_32w[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_ctr_destination_data_32w[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    //CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_CCM_CTR, crypto_aes256_count);
    CRYPTO_AES_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes_ctr_destination_data_32w, (16*4), (uint32_t *)encrypto_aes_result_data_32w);

    for(i = 0; i < 16; i++){
        if(encrypto_aes256_raw_data[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes256_raw_data[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes256_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

__attribute__((aligned(32))) static unsigned char aes_raw_str[] =
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

uint32_t encrypto_aes_ctr_destination_str[] =
{
    0x134bbc89, 0xca330d2b, 0x0d145d5c, 0xfeb414a1, 0x6dc71da8, 0x6e0e974a, 0xba7a00b9, 0x209874ae, 0xadd47052, 0xea2ac989, 0xa9d8c58f, 0xdc133f63,
    0xf96c3822, 0x65be0bbf, 0xbe5c3ccc, 0xac52e58f, 0x4fe19520, 0xcf774a69, 0x3e501a9e, 0x739c06d6, 0x906fcf15, 0x16251a66, 0x91af4f53, 0xa3e47f25, 0x2e8b10ff, 0x85632ffc, 0xd178e0b0, 0x5f50e618,
    0x2314c3b3, 0x91ff7225, 0x7bcc1cf6, 0xacc5bec2, 0x136717e2, 0xfbed9312, 0x98373939, 0x6ed99c36, 0xd1dae934, 0xe59eddad, 0x972d5428, 0xef43563b, 0x6d95d91d, 0x58d587ec, 0x5156d12b, 0xf10e06f0,
    0x90f48f43, 0x195c5b16, 0x2c18888a, 0x23fd33ed, 0xb1587ae0, 0x5825bc64, 0x7ff994d8, 0x66c87b66, 0xb19d2820, 0xfb51e36d, 0xa4d3b8cb, 0x871c4c04, 0x23600879, 0x06344852, 0xf849a223, 0x6120fef6,
    0x7346343a, 0xd74d5e47, 0x598b87f9, 0x9e09dd36, 0x0fb78b4d, 0x813b86e9, 0x4a09f6fe, 0x45d1da3d, 0x822ce10b, 0x96873588, 0x492e56d4, 0xc10cba75, 0xe04f7ca4, 0xd8fe3c0a, 0xc77cc33e, 0xa32ee01d,
    0x53030079, 0x1b4f5ac9, 0xf4fd5790, 0x72656dfc, 0x88171da2, 0x6f18f34a, 0x8688978e, 0xa7cc9586, 0x106a9673, 0x703689cf, 0xb7c45d96, 0x6a0495f4, 0xa4b7f999, 0x7420efc8, 0x3c05c44b, 0x896d6e52,
    0xf5537aee, 0xd8f8435f, 0x4c213c19, 0x78b12d97, 0x6d070ae0, 0x52dec7da, 0xb1903f3a, 0x84bdc303, 0xa9788ec3, 0xc4ba06b5, 0x5dc1a4de, 0x6975a276, 0x8d8ccaec, 0x75029ce6, 0x54e3f66b, 0x458d2ad8,
    0xc6632f31, 0xf7432eb1, 0x5b8487b1, 0x8e345147, 0x62c95112, 0x3ee9888b, 0xfcb16c13, 0x42e862a6, 0xabe0060d, 0x37da4aa6, 0x56fcad64, 0xf1e516f5, 0xf3812fea, 0x28cf3225, 0xe4b7290a, 0x30307bc7,
    0x645a4b48, 0x0c837c1a, 0xf7fd7504, 0xbf3b8df3, 0xa8cb6500, 0x0256c493, 0x6b935d35, 0x7087af77, 0x5d50fca2, 0x73f9fc8d, 0x4f763ed1, 0x9e60df55, 0xa17903c2, 0x2b064aa7, 0x97aba252, 0x4bf4d148,
    0x2b47d0e0, 0xc3bcc679, 0x0a139162, 0x0d002969, 0x9f06ebaa, 0x9607b3c4, 0xdc55f365, 0xca4dc8b2, 0xb513c07e, 0x37b0a246, 0x82001420, 0xf0b81d6c, 0x82eb8566, 0x9cffac8f, 0xbdf68096, 0xab9a2ff7,
    0xd06e5fe0, 0xf6b8673c, 0x7077745f, 0x233cc367, 0x98bd2117, 0xecc18c40, 0xbad6cf04, 0x5e5ceac4, 0xf8358cc1, 0xecaca544, 0x71dd8283, 0x1dfa67da, 0xd3c1f3da, 0x6577326a, 0x9575843a, 0xbd52900a,
    0xc326f579, 0x87bbf7c7, 0x5f6e8847, 0x0e5325e2, 0xaecc1ea8, 0x199197df, 0xe51c987b, 0x1867cc25, 0x7add1c72, 0x96ba3c94, 0xbbf7ca82, 0xe6070e59, 0x904eab50, 0xbc0181b1, 0x44a7de6b, 0x3ae58f1d,
    0xcd24f1d2
};

void CRYPTO0_AES256_CTR_Encrypt_Efuse_User_Key()
{
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint32_t aes_length[4] = {16, 16, 0, sizeof(aes_raw_str)};
    uint32_t data_len = sizeof(aes_raw_str);
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;


    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_256, 0);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_CTR);

    //CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_EFUSE_USER, 120); // crypto_aes256_user_key
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes256_user_key);

    for(int n=1; n<6; n++){
        data_len = sizeof(aes_raw_str)/n;
        systime_1 = SysTick_Value();
    aes_length[3] = data_len;
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_IV, (uint32_t)crypto_aes256_iv);

    memset(encrypto_aes_result_data_32w, 0, sizeof(encrypto_aes_result_data_32w));

    CRYPTO_AES_Encrypt(CRYPTO0_Handler, (uint32_t *)aes_raw_str, data_len, (uint32_t *)encrypto_aes_result_data_32w);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    int i = 0;
    int err_count = 0;

    for(i = 0; i < (data_len+3)/4; i++){
        if(encrypto_aes_ctr_destination_str[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_ctr_destination_str[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: Compare finish!, error/total words: %d/%d, use time %dus", err_count, (data_len+3)/4, systime_diff/CRYPTO_MAIN_FREQ);

    systime_1 = SysTick_Value();
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    //CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_CCM_CTR, crypto_aes256_count);
    CRYPTO_AES_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes_ctr_destination_str, data_len, (uint32_t *)encrypto_aes_result_data_32w);

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    err_count = 0;
    uint32_t *target_raw_data = (uint32_t *)aes_raw_str;
    for(i = 0; i < (data_len+3)/4; i++){
        if(target_raw_data[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%x", i, target_raw_data[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, target_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CLOGD("I: Compare finish!, error/total words: %d/%d, use time %dus", err_count, (data_len+3)/4,systime_diff/CRYPTO_MAIN_FREQ);
    }// end for
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

static uint32_t encrypto_aes_ccm128_destination_data_32w[] = {
        0x35b4e5d8, 0xd6ef7fd0, 0x325b6e26, 0x32320c65, 0x267bc85b, 0xb3545366, 0xf17656bd, 0x9f531590,
        0x8eecf525, 0x138535f3, 0xc7d37ded, 0x269615f0, 0x6c498e81, 0xc85bcb04, 0xc9ace047, 0x789c4ac5,
        0x3deb2e32, 0x9837323b, 0x47eded76, 0x88c83d6f, 0x7bc912ab, 0x3f072252, 0x44701483, 0x2b557873,
        0x7e58247a, 0x1c786b22, 0xe488d120, 0xcf098d4a, 0xe9e7542c, 0x68a3df8a, 0xe0bf92cc, 0xe44d288a,
        0xf84a277a, 0x69c2672d, 0x1e55cf0d, 0x06b045fe, 0x12e9327d, 0x0bbd27c0, 0xd7dc486f, 0x986d6f98,
        0x38eeb509, 0xa02c43ac, 0x2e2786c3, 0xf80518db, 0x055609b1, 0x56e34cd8, 0x7d24d9b6, 0x44194312,
        0x4eb84fc2, 0x84c7668e, 0xa1b24f3b, 0xbd423312, 0xf24dca4a, 0xdc805861, 0x110e019e, 0x33e7165f,
        0x548b697e, 0x3f65f8d4, 0x535ebff7, 0x15b1b2fa, 0x82737615, 0x4cf591af, 0x3b261076, 0xe7909800,
        0x726a2d28, 0x0b22decf, 0x5cac16d8, 0xc22e5fbf, 0xf37dba22, 0xd3c41ee7, 0x2d3c8a18, 0xa47276d6,
        0x25244816, 0x95f80ef7, 0xa2cc4348, 0x31740d98, 0xfc871828, 0xa5e2479c, 0x8f806b7c, 0x76dbca17,
        0xc6e25113, 0x034f6766, 0xc01ff8b8, 0x3e4193bd, 0x16f7e1cd, 0x058162f6, 0x8ca2347d, 0x0e7e8395,
        0xdc10d94c, 0xf4985df5, 0xc800fdce, 0x4c42da15, 0x677ffeba, 0xad6a69d6, 0x91b6bf42, 0x9f472059,
        0x589d8bee, 0x5f1ea5e9, 0xf9d5f0fb, 0xa32fb143, 0x733812bd, 0xe4be886c, 0x1f997f1f, 0x27374dd5,
        0x7df1a8f9, 0x2767986c, 0x370d69ce, 0x50dce46a, 0x482498dd, 0x25b366aa, 0x6fe2839d, 0xaaf2cd45,
        0xd78f3b7a, 0xe642984e, 0xca93f060, 0xae331cd0, 0x437b1019, 0xe31160b1, 0x3362289e, 0x01a34cf4,
        0x73584c6c, 0x263d464f, 0xe24a49f8, 0x51c610ba, 0x6c19f692, 0x602501cf, 0x41819105, 0x1075d5a5,
        0x5f6eda15, 0x27c3f719, 0x1b6703f4,
        0xdd8331ad, // mac
        0xc8fe2fb5,
        0xcb9fd8f7,

};

void CRYPTO0_AES128_CCM_Encrypt_User_Key()
{
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint32_t aes_length[4] = {12, 10, 187, 521};
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;
    uint32_t *p_raw_data = (uint32_t*)& aes_raw_str[128];
    int err_count = 0;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    systime_1 = SysTick_Value();
    memset(encrypto_aes_result_data_32w, 0, sizeof(encrypto_aes_result_data_32w));
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_128, 0);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_CCM);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes_user_key);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)encrypto_aes_cbc_iv);

    // aad
    CRYPTO_AES_Encrypt(CRYPTO0_Handler, (uint32_t *)aes_raw_str, aes_length[2], (uint32_t *)encrypto_aes_result_data_32w);

    // data
    CRYPTO_AES_Encrypt(CRYPTO0_Handler, (uint32_t *)p_raw_data, aes_length[3], (uint32_t *)encrypto_aes_result_data_32w);

    // get mac
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_GET_AES_MAC, (uint32_t)(encrypto_aes_result_data_32w+(aes_length[3]+3)/4));

    int i = 0;
    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("AES128 CCM test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < (aes_length[3]+3)/4+3; i++){
        if(encrypto_aes_ccm128_destination_data_32w[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_ccm128_destination_data_32w[i], encrypto_aes_result_data_32w[i]);
        }
    }

    systime_1 = SysTick_Value();
    memset(encrypto_aes_result_data_32w, 0, sizeof(encrypto_aes_result_data_32w));
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)encrypto_aes_cbc_iv);

    // aad
    CRYPTO_AES_Decrypt(CRYPTO0_Handler, (uint32_t *)aes_raw_str, aes_length[2], (uint32_t *)encrypto_aes_result_data_32w);

    CRYPTO_AES_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes_ccm128_destination_data_32w, aes_length[3], (uint32_t *)encrypto_aes_result_data_32w);

    // get mac
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_GET_AES_MAC, (uint32_t)(encrypto_aes_result_data_32w+(aes_length[3]+3)/4));

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("AES128 CCM test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);
    err_count = 0;
    for(i = 0; i < (aes_length[3]+3)/4; i++){
        if(p_raw_data[i] == encrypto_aes_result_data_32w[i]){
            if((i%10)==0)
                CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            err_count ++;
            if(err_count < 10)
                CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, p_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }
    for(; i < (aes_length[3]+3)/4+3; i++){
        if(encrypto_aes_ccm128_destination_data_32w[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_ccm128_destination_data_32w[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}


static uint32_t aes256_ccm_nonce[] = {0xc4434076,0x29b76094,0x8466d0ee,0}; //{0x43407606, 0xb76094c4, 0x29, 0x30000000};

static uint32_t encrypto_aes_ccm_destination_data_32w[] = {
        0x9e5b342a,        0xc48744fc,        0x95f24966,        0xd4be210b,
        0x3676ad5e,        0x97b030f5,        0x424754e8,        0x1bf0c106,
        0x1ee8bffd,        0xe1c68957,        0x10f310ae,        0xa4a5b446,
        0x8b4cd6c0,        0xc79f1d8a,
        0x9790dfe0,// mac
        0x2b115894,
        0x92115979,
        0xc7ad0c90,
};

void CRYPTO0_AES256_CCM_Encrypt_User_Key()
{
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint32_t aes_length[4] = {16, 12, 8, 56};
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    memset(encrypto_aes_result_data_32w, 0, sizeof(encrypto_aes_result_data_32w));
    systime_1 = SysTick_Value();
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_256, 0);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_CCM);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes256_user_key);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)aes256_ccm_nonce);

    CRYPTO_AES_Encrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_raw_data, 64, (uint32_t *)encrypto_aes_result_data_32w);

    // get mac
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_GET_AES_MAC, (uint32_t)(encrypto_aes_result_data_32w+14));

    int i = 0;
    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("AES256 CCM encrypt test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 18; i++){
        if(encrypto_aes_ccm_destination_data_32w[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_ccm_destination_data_32w[i], encrypto_aes_result_data_32w[i]);
        }
    }

    systime_1 = SysTick_Value();
    memset(encrypto_aes_result_data_32w, 0, sizeof(encrypto_aes_result_data_32w));
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)aes256_ccm_nonce);

    // aad
    CRYPTO_AES_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_raw_data, (8), (uint32_t *)encrypto_aes_result_data_32w);

    CRYPTO_AES_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes_ccm_destination_data_32w, (56), (uint32_t *)encrypto_aes_result_data_32w);

    // get mac
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_GET_AES_MAC, (uint32_t)(encrypto_aes_result_data_32w+14));

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("AES256 CCM decrypt test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 14; i++){
        if(encrypto_aes256_raw_data[i+2] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes256_raw_data[i+4], encrypto_aes_result_data_32w[i]);
        }
    }
    for(; i < 18; i++){
        if(encrypto_aes_ccm_destination_data_32w[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes256_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

static uint32_t encrypto_aes_gcm192_destination_data_32w[] = {
    0xe6534b0a,    0x47cc77de,    0x8444922f,    0x0e2bb7c6,
    0x853c58b3,    0xe0e56434,    0x62f60878,    0x6e261cad,
    0x9c20d3b2,    0x12d7486a,    0x65af2520,    0xb4d8d8ee,
    0x96ab6d81,    0x72eb0af6,    0x45aea50c,    0xd3e1fe13,
    0x36bf304f,    0xd7a6818e,    0x40aea784,    0xe3733513,
    0x52c14a52,    0x296a67d2,    0xe37cc8b1,    0x47e6b225,
    0x59941232,    0x0c653e7c,
    0x47b3bcfd, // mac
    0xe1f252e1,
    0xbd7445f5,
    0x8647415d,
};

void CRYPTO0_AES192_GCM_Encrypt_User_Key()
{
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint32_t aes_length[4] = {16, 12, 24, 101};
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    memset(encrypto_aes_result_data_32w, 0, sizeof(encrypto_aes_result_data_32w));
    systime_1 = SysTick_Value();

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_192, 0);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_GCM);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    //CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_EFUSE2, 0); // crypto_aes256_user_key
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes256_user_key);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)aes256_ccm_nonce);

    CRYPTO_AES_Encrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes_source_data_32w, aes_length[2]+aes_length[3], (uint32_t *)encrypto_aes_result_data_32w);

    // get mac
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_GET_AES_MAC, (uint32_t)(encrypto_aes_result_data_32w+aes_length[3]/4+1));

    int i = 0;
    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("AES192 GCM encrypt test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < aes_length[3]/4+5; i++){
        if(encrypto_aes_gcm192_destination_data_32w[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_gcm192_destination_data_32w[i], encrypto_aes_result_data_32w[i]);
        }
    }

    systime_1 = SysTick_Value();
    memset(encrypto_aes_result_data_32w, 0, sizeof(encrypto_aes_result_data_32w));
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)aes256_ccm_nonce);

    // aad
    CRYPTO_AES_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes_source_data_32w, aes_length[2], (uint32_t *)encrypto_aes_result_data_32w);

    CRYPTO_AES_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes_gcm192_destination_data_32w, aes_length[3], (uint32_t *)encrypto_aes_result_data_32w);

    // get mac
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_GET_AES_MAC, (uint32_t)(encrypto_aes_result_data_32w+aes_length[3]/4+1));

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("AES192 GCM decrypt test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < aes_length[3]/4+1; i++){
        if(encrypto_aes_source_data_32w[i+aes_length[2]/4] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_source_data_32w[i+4], encrypto_aes_result_data_32w[i]);
        }
    }
    for(; i < aes_length[3]/4+5; i++){
        if(encrypto_aes_gcm192_destination_data_32w[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes_gcm192_destination_data_32w[i], encrypto_aes_result_data_32w[i]);
        }
    }
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

uint32_t encrypto_aes256_gcm_destination_data[] = {
        0x9d85f928, // aad
        0x8bb1cb7e,
        0xc4780008,
        0xa1682a71,
        0x62d7f117,
        0xd4e22c79,
        0xa606889c,
        0x47a9db68,
        0x403f5e95, // encrypt data
        0x5c6eae12,
        0xe71feb07,
        0xe5afbf87,
        0xf22dca92,
        0x45756f72,
        0x8f2d3ebc,
        0xc0b72f11,
        0x387a6057,
        0xe54fdcca,
        0x008e3983,
        0xa154f426,
        0x76d3b2af,
        0x1335f18e,
        0x30bc97f5,
        0x14ae4153,
        0xc84a3f5d, // mac
        0x0298dc2d,
        0x8888c095,
        0x1ffe5c25,

};
void CRYPTO0_AES256_GCM_Encrypt_User_Key()
{
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint32_t aes_length[4] = {16, 12, 32, 16*4};
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    memset(encrypto_aes_result_data_32w, 0, sizeof(encrypto_aes_result_data_32w));
    systime_1 = SysTick_Value();

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_256, 0);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_GCM);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes256_user_key);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)aes256_ccm_nonce);

    // aad
    CRYPTO_AES_Encrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_gcm_destination_data, 32, (uint32_t *)encrypto_aes_result_data_32w);

    // raw data
    CRYPTO_AES_Encrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_raw_data, (16*4), (uint32_t *)encrypto_aes_result_data_32w);

    // get mac
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_GET_AES_MAC, (uint32_t)(encrypto_aes_result_data_32w+16));

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    int i = 0;

    CLOGD("AES256 GCM encrypt test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 16+4; i++){
        if(encrypto_aes256_gcm_destination_data[i+8] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes256_gcm_destination_data[i+8], encrypto_aes_result_data_32w[i]);
        }
    }

    systime_1 = SysTick_Value();
    memset(encrypto_aes_result_data_32w, 0, sizeof(encrypto_aes_result_data_32w));
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)aes256_ccm_nonce);
    CRYPTO_AES_Decrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_gcm_destination_data, 32+(16*4), (uint32_t *)encrypto_aes_result_data_32w);

    // get mac
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_GET_AES_MAC, (uint32_t)(encrypto_aes_result_data_32w+16));

    systime_2 = SysTick_Value();
    systime_diff = systime_2 - systime_1;

    CLOGD("AES256 GCM decrypt test finished, use time %dus", systime_diff/CRYPTO_MAIN_FREQ);

    for(i = 0; i < 16; i++){
        if(encrypto_aes256_raw_data[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes256_raw_data[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes256_raw_data[i], encrypto_aes_result_data_32w[i]);
        }
    }
    for(i = 0; i < 4; i++){
        if(encrypto_aes256_gcm_destination_data[i+16+8] == encrypto_aes_result_data_32w[i+16]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i+16]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_aes256_gcm_destination_data[i+16+8], encrypto_aes_result_data_32w[i+16]);
        }
    }
    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);

}


uint32_t aes128_cmac_destination_mac[] = {0xa14bd25a, 0x278cf1dd, 0x3a1ab41f, 0x45de9c69 };

void CRYPTO0_AES128_CMAC_Encrypt_User_Key()
{
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint32_t aes_length[4] = {16, 0, 0, sizeof(encrypto_aes256_raw_data)};

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_128, 0);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_CMAC);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    //CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_EFUSE2, 0); // crypto_aes256_user_key
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes256_user_key);

    CRYPTO_AES_Encrypt(CRYPTO0_Handler, (uint32_t *)encrypto_aes256_raw_data, aes_length[3], (uint32_t *)encrypto_aes_result_data_32w);

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_GET_AES_MAC, (uint32_t)encrypto_aes_result_data_32w);

    int i = 0;

    for(i = 0; i < 4; i++){
        if(aes128_cmac_destination_mac[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, aes128_cmac_destination_mac[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);

}

uint32_t aes192_cmac_destination_mac[] = {0x41e0e60b, 0xa1d6a655, 0x16d9860c, 0x413c325e };

void CRYPTO0_AES192_CMAC_Encrypt_User_Key()
{
    uint32_t aes_length[4] = {16, 0, 0, sizeof(aes_raw_str)};
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_192, 0);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_CMAC);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes256_user_key);

    CRYPTO_AES_Encrypt(CRYPTO0_Handler, (uint32_t *)aes_raw_str, sizeof(aes_raw_str), NULL);

    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_GET_AES_MAC, (uint32_t)encrypto_aes_result_data_32w);

    int i = 0;

    for(i = 0; i < 4; i++){
        if(aes192_cmac_destination_mac[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, aes192_cmac_destination_mac[i], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

uint32_t aes256_cmac_destination_mac[] = {
        0x339e1c55, 0xe9d4dd68, 0xb7d6aa3a, 0xbe79bd0c,
        0x678c4351, 0xd440c15f, 0x795cdc57, 0x2df22867,
        0x7f6973d1, 0xe89179ef, 0xf5a1fb31, 0x9ffbd28f,
        0x298b404d, 0x157cd08b, 0xe1d04e33, 0x26ab54a3,
        0x9a01cdbc, 0x2fc7dcff, 0x6cac3c15, 0xad4754c6,
        0xcaeb0d81, 0xc11434d9, 0xf9a9bea8, 0x4865e750,
        0x62f3531e, 0x20db8f5f, 0xff56db72, 0x231abc19,
        0x77b4fb82, 0xd3174eb5, 0xe81e6397, 0xd994d5ce,
};

void CRYPTO0_AES256_CMAC_Encrypt_User_Key()
{
    int n;
    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint32_t aes_length[4] = {16, 0, 0, sizeof(aes_raw_str)};
    uint32_t systime_1 = 0;
    uint32_t systime_2 = 0;
    uint32_t systime_diff = 0;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_KEY_SIZE_256, 0);
    CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_CMAC);
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)crypto_aes256_user_key);

    for(n=1; n<9; n++) {
//        if((n&3)==0)
//            aes_length[0] = 16-n;
//        else
            aes_length[0] = 16;
        aes_length[3] = sizeof(aes_raw_str)/n;
        CLOGD("I: CMAC test, data_len=%d, mac_len=%d", aes_length[3], aes_length[0]);

        CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
        systime_1 = SysTick_Value();
        CRYPTO_AES_Encrypt(CRYPTO0_Handler, (uint32_t *)aes_raw_str, sizeof(aes_raw_str)/n, NULL);

        CRYPTO_Control(CRYPTO0_Handler,CSK_CRYPTO_GET_AES_MAC, (uint32_t)&encrypto_aes_result_data_32w[4*(n-1)]);
        systime_2 = SysTick_Value();
        systime_diff = systime_2 - systime_1;
        CLOGD("I: CMAC test with data length %d finished, use time %dus", aes_length[3], systime_diff/CRYPTO_MAIN_FREQ);
    }

    int i = 0;

    for(i = 0; i < 32; i++){
        if(aes256_cmac_destination_mac[i] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, aes256_cmac_destination_mac[i], encrypto_aes_result_data_32w[i]);
        }
    }


    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

