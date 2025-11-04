/*
 * crypto_nos_chk.h
 *
 *  Created on: 2023/5/22
 *      Author: LAPTOP-07
 */

#ifndef SRC_DRV_DEMO_DRV_DEMO_ARCS_CRYPTO_CRYPTO_NOS_CHK_H_
#define SRC_DRV_DEMO_DRV_DEMO_ARCS_CRYPTO_CRYPTO_NOS_CHK_H_

// frequency in MHz
#define CRYPTO_MAIN_FREQ        1

extern void* CRYPTO0_Handler;

extern uint32_t encrypto_aes_result_data_32w[200];
//extern uint32_t *encrypto_aes_result_data_32w;

void CRYPTO0_AES_Write_Efuse_Key();

void CRYPTO0_EventCallback(uint32_t event, void* workspace);

// aes test functions
void CRYPTO0_AES128_ECB_Encrypt_User_Key();
void CRYPTO0_AES128_ECB_Decrypt_User_Key();
void CRYPTO0_AES192_ECB_Encrypt_User_Key();

void CRYPTO0_AES128_Decrypt_Flash_Test();
void CRYPTO0_AES128_Decrypt_PSRAM_Test();
void CRYPTO0_PSRAM_Cihper_Region_Validation();

void CRYPTO0_AES256_CBC_Encrypt_User_Key();
void CRYPTO0_AES256_CBC_Encrypt_Efuse2_Key();

void CRYPTO0_AES192_CBC_Encrypt_User_Key();
void CRYPTO0_AES192_CBC_Decrypt_User_Key();

void CRYPTO0_AES192_CTR_Encrypt_User_Key();
void CRYPTO0_AES256_CTR_Encrypt_Efuse_User_Key();
void CRYPTO0_AES128_CCM_Encrypt_User_Key();
void CRYPTO0_AES256_CCM_Encrypt_User_Key();
void CRYPTO0_AES128_CMAC_Encrypt_User_Key();
void CRYPTO0_AES256_CMAC_Encrypt_User_Key();
void CRYPTO0_AES192_CMAC_Encrypt_User_Key();
void CRYPTO0_AES192_GCM_Encrypt_User_Key();
void CRYPTO0_AES256_GCM_Encrypt_User_Key();

// sha test functions
void CRYPTO0_SHA1_LittleEndian_Test();
void CRYPTO0_SHA224_LittleEndian_Test();
void CRYPTO0_SHA256_LittleEndian_Test();

void CRYPTO0_SHA224_LittleEndian_LongStream_Test(void);
void CRYPTO0_SHA256_LittleEndian_LongStream_Light_Test(void);

void CRYPTO0_SHA384_LittleEndian_Test();
void CRYPTO0_SHA512_LittleEndian_Test();

void CRYPTO0_SHA384_LittleEndian_LongStream_Test(void);
void CRYPTO0_SHA512_LittleEndian_LongStream_Test(void);

void CRYPTO0_HMAC_SHA1_LittleEndian_Test(void);
void CRYPTO0_HMAC_SHA256_LittleEndian_Test(void);
void CRYPTO0_HMAC_SHA512_LittleEndian_Test(void);

void CRYPTO0_HMAC_SHA224_LittleEndian_LongStream_Test(void);
void CRYPTO0_HMAC_SHA384_LittleEndian_LongStream_Test(void);

// ecc test functions
void CRYPTO0_MOD_OPERATE_512_LittleEndian_Test();

void CRYPTO0_ECC_P192_Generate_Key();
void CRYPTO0_ECC_P224_Generate_Key();
void CRYPTO0_ECC_P512_Generate_Key();
void CRYPTO0_ECC_USER_CURVE_Generate_Key(); // 320bit

void CRYPTO0_ECC_P224_ADD();
void CRYPTO0_ECC_P256_Multiply();
void CRYPTO0_ECC_USER_CURVE_Multiply(); // 160bit

void CRYPTO0_ECC_ECDH192_Test();
void CRYPTO0_ECC_ECDH384_Test();

void CRYPTO0_ECC_ECSDA256_Verify_Signature();
void CRYPTO0_ECC_ECSDA256_Flash_Verify_Signature();

// rsa test functions
void CRYPTO0_RSA1024_Encrypt_BigEndian();
void CRYPTO0_RSA2048_Encrypt_BigEndian();
void CRYPTO0_RSA4096_Encrypt_BigEndian();
void CRYPTO0_RSA2048_Signature_BigEndian();
void CRYPTO0_MOD_EXP_Little_Endian();
void CRYPTO0_ECC_RSA2048_Flash_Verify_Signature();

// hsu test fucntions
void CRYPTO0_HSU_TKIP_Test1();
void CRYPTO0_HSU_TKIP_Test2();
void CRYPTO0_HSU_TKIP_Test3();
void CRYPTO0_HSU_TKIP_Test4();
void CRYPTO0_HSU_IP_CHK_Test();
void CRYPTO0_HSU_IP_CHK_Test_bg();

#endif /* SRC_DRV_DEMO_DRV_DEMO_ARCS_CRYPTO_CRYPTO_NOS_CHK_H_ */
