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

uint8_t encrypto_hsu_tkip_source_data1 [] = {
        0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0xA8, 0x1E, 0x84, 0x0D, 0xC8, 0xDD, 0xC0, 0xA8, 0x02, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xC0, 0xA8, 0x02, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

uint32_t encrypto_hsu_tkip_key1[] = {
        0xe9491160, 0xca2bc744, 0
};

uint32_t encrypto_hsu_tkip_result1[] = {
        0xd3bbdc98, 0x5e805838, 0
};


uint8_t encrypto_hsu_tkip_source_data2 [] = {
        0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00, 0x00, 0x3C, 0x2A, 0x51, 0x00, 0x00, 0x80, 0x01, 0x8A, 0x54, 0xC0, 0xA8, 0x02, 0x66, 0xC0, 0xA8, 0x02, 0x65, 0x08, 0x00, 0xED, 0xEA,
        0x00, 0x01, 0x5F, 0x70, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x61, 0x62, 0x63, 0x64, 0x65,
        0x66, 0x67, 0x68, 0x69};

uint32_t encrypto_hsu_tkip_key2[] = {
        0x60e6bfa4, 0x78966131, 0
};

uint32_t encrypto_hsu_tkip_result2[] = {
        0x1d513ef5, 0x73e2aeda, 0
};



void CRYPTO0_HSU_TKIP_Test1(){

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    // 1st loop
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_MIC_KEY, (uint32_t)encrypto_hsu_tkip_key1);

    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data1, sizeof(encrypto_hsu_tkip_source_data1), 1, encrypto_aes_result_data_32w);

    // 2nd loop
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_MIC_KEY, (uint32_t)encrypto_hsu_tkip_key1);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data1, 13, 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data1+13, sizeof(encrypto_hsu_tkip_source_data1)-13, 1, encrypto_aes_result_data_32w+2);

    // 3th loop
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_MIC_KEY, (uint32_t)encrypto_hsu_tkip_key1);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data1, 16, 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data1+16, 22, 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data1+38, sizeof(encrypto_hsu_tkip_source_data1)-38, 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, NULL, 0, 1, encrypto_aes_result_data_32w+4);

    int i = 0;

    for(i = 0; i < 6; i++){
        if(encrypto_hsu_tkip_result1[i&1] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_hsu_tkip_result1[i&1], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}


void CRYPTO0_HSU_TKIP_Test2(){

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    // 1st loop
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_MIC_KEY, (uint32_t)encrypto_hsu_tkip_key2);

    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data2, sizeof(encrypto_hsu_tkip_source_data2), 1, encrypto_aes_result_data_32w);

    // 2nd loop
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_MIC_KEY, (uint32_t)encrypto_hsu_tkip_key2);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data2, 32, 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data2+32, sizeof(encrypto_hsu_tkip_source_data2)-32, 1, encrypto_aes_result_data_32w+2);

    // 3th loop
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_MIC_KEY, (uint32_t)encrypto_hsu_tkip_key2);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data2, 23, 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data2+23, 32, 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data2+55, sizeof(encrypto_hsu_tkip_source_data2)-55, 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, NULL, 0, 1, encrypto_aes_result_data_32w+4);


    int i = 0;

    for(i = 0; i < 6; i++){
        if(encrypto_hsu_tkip_result2[i&1] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_hsu_tkip_result2[i&1], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}


uint8_t encrypto_hsu_tkip_source_data3 [] = {
        0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0x20, 0x7B, 0xD2, 0x56, 0x29, 0x0F, 0xC0, 0xA8, 0x02, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xC0, 0xA8, 0x02, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

uint32_t encrypto_hsu_tkip_aad3[] = {
        0xffffffff, 0x7b20ffff, 0xf2956d2, 0
};

uint32_t encrypto_hsu_tkip_key3[] = {
        0xc42501ad, 0xd4b38493, 0
};

uint32_t encrypto_hsu_tkip_result3[] = {
        0x8f6f3ea4, 0x0c1bb516, 0
};


uint8_t encrypto_hsu_tkip_source_data4 [] = {
        0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00, 0x00, 0x3C, 0xE3, 0xE3, 0x00, 0x00, 0x80, 0x01, 0xD0, 0xBF, 0xC0, 0xA8, 0x02, 0x66, 0xC0, 0xA8, 0x02, 0x67, 0x08, 0x00, 0xAE, 0xE5,
        0x00, 0x01, 0x9E, 0x75, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x61, 0x62, 0x63, 0x64, 0x65,
        0x66, 0x67, 0x68, 0x69
};

uint32_t encrypto_hsu_tkip_aad4[] = {
        0x257fa400, 0x7b201c61, 0xf2956d2, 0
};

uint32_t encrypto_hsu_tkip_key4[] = {
        0x8f42c9f4, 0x8252703a, 0
};

uint32_t encrypto_hsu_tkip_result4[] = {
        0x8fe8710e, 0x8291a7d4, 0
};

void CRYPTO0_HSU_TKIP_Test3(){

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    // 1st loop
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_MIC_KEY, (uint32_t)encrypto_hsu_tkip_key3);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, (uint8_t*)encrypto_hsu_tkip_aad3, sizeof(encrypto_hsu_tkip_aad3), 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data3, sizeof(encrypto_hsu_tkip_source_data3), 1, encrypto_aes_result_data_32w);

    // 2nd loop
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_MIC_KEY, (uint32_t)encrypto_hsu_tkip_key3);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, (uint8_t*)encrypto_hsu_tkip_aad3, sizeof(encrypto_hsu_tkip_aad3), 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data3, 16, 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data3+16, sizeof(encrypto_hsu_tkip_source_data3)-16, 1, encrypto_aes_result_data_32w+2);

    int i = 0;

    for(i = 0; i < 4; i++){
        if(encrypto_hsu_tkip_result3[i&1] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_hsu_tkip_result3[i&1], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

void CRYPTO0_HSU_TKIP_Test4(){

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

    // 1st loop
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_MIC_KEY, (uint32_t)encrypto_hsu_tkip_key4);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, (uint8_t*)encrypto_hsu_tkip_aad4, sizeof(encrypto_hsu_tkip_aad4), 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data4, sizeof(encrypto_hsu_tkip_source_data4), 1, encrypto_aes_result_data_32w);

    // 2nd loop
    CRYPTO_Control(CRYPTO0_Handler, CSK_CRYPTO_SET_MIC_KEY, (uint32_t)encrypto_hsu_tkip_key4);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, (uint8_t*)encrypto_hsu_tkip_aad4, sizeof(encrypto_hsu_tkip_aad4), 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data4, 32, 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data4+32, 16, 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, encrypto_hsu_tkip_source_data4+48, sizeof(encrypto_hsu_tkip_source_data4)-48, 0, NULL);
    CRYPTO_TKIP_Michael(CRYPTO0_Handler, NULL, 0, 1, encrypto_aes_result_data_32w+2);
    int i = 0;

    for(i = 0; i < 4; i++){
        if(encrypto_hsu_tkip_result4[i&1] == encrypto_aes_result_data_32w[i]){
            CLOGD("I: %d compare success!, data: 0x%x", i, encrypto_aes_result_data_32w[i]);
        }else{
            CLOGD("I: %d compare failed!, destination data: 0x%x != result data: 0x%x", i, encrypto_hsu_tkip_result4[i&1], encrypto_aes_result_data_32w[i]);
        }
    }

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
}

uint8_t encrypto_hsu_ip_chk_source_data1 [] = {
    0x45, 0x00, 0x00, 0x3C, 0x2A, 0xB7, 0x00, 0x00, 0x80, 0x01, 0x89, 0xEE, 0xC0, 0xA8, 0x02, 0x66, 0xC0, 0xA8, 0x02, 0x65
};

uint8_t encrypto_hsu_ip_chk_source_data2 [] = {
    0x45, 0x00, 0x00, 0x3C, 0x2A, 0xB7, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0xC0, 0xA8, 0x02, 0x66, 0xC0, 0xA8, 0x02, 0x65
};

void CRYPTO0_HSU_IP_CHK_Test(){

    CLOGD("Test Case %d ----> %s", __LINE__ ,__FUNCTION__);
    uint16_t checksum;

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_IP_CHECKSUM, CSK_POWER_FULL);

    CRYPTO_IP_Checksum(CRYPTO0_Handler, encrypto_hsu_ip_chk_source_data1, sizeof(encrypto_hsu_ip_chk_source_data1), &checksum);
    if((checksum^0xffff) != 0x0000)
        CLOGD("I: ip checksum result: 0x%04x, shuould be 0x0000", checksum^0xffff);

    CRYPTO_IP_Checksum(CRYPTO0_Handler, encrypto_hsu_ip_chk_source_data2, sizeof(encrypto_hsu_ip_chk_source_data2), &checksum);
    if((checksum^0xffff) != 0xee89)
        CLOGD("I: ip checksum result: 0x%04x, shuould be 0xee89", checksum^0xffff);

    CRYPTO_PowerControl(CRYPTO0_Handler, CSK_CRYPTO_HW_IP_CHECKSUM, CSK_POWER_OFF);
}

