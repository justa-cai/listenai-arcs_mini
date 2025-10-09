
#include "log_print.h"
#include "Driver_CRYPTO.h"

int32_t bt_p192_private_public_key_gen(uint32_t seed, uint32_t* ecc_private_key1, uint32_t*ecc_public_key1)
{
    int32_t status;
    uint32_t t_ecc_private_key1[6];
    uint32_t t_ecc_public_key1[12];
    //CLOGI("seed:0x%x", seed);
    CRYPTO_PowerControl(CRYPTO0(), CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0(), CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P192);
    status =  CRYPTO_ECC_Generate_Key(CRYPTO0(), seed, t_ecc_private_key1, t_ecc_public_key1);

    CRYPTO_SWAP_Bytes(CRYPTO0(), t_ecc_private_key1, sizeof(t_ecc_private_key1), ecc_private_key1);
    CRYPTO_SWAP_Bytes(CRYPTO0(), t_ecc_public_key1,   sizeof(t_ecc_public_key1)/2,  ecc_public_key1);
    CRYPTO_SWAP_Bytes(CRYPTO0(), t_ecc_public_key1+6,   sizeof(t_ecc_public_key1)/2,  ecc_public_key1+6);

    CRYPTO_PowerControl(CRYPTO0(), CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);

    return status;
}

int32_t bt_p192_dh_key_gen(uint32_t* ecc_private_key1, uint32_t*ecc_remote_public_key1, uint32_t *ecc_dh_key)
{
    int32_t status;
    uint32_t t_ecc_private_key1[6];
    uint32_t t_ecc_public_key1[12];
    uint32_t t_ecc_dh_key1[12];

    CRYPTO_PowerControl(CRYPTO0(), CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0(), CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P192);

    CRYPTO_SWAP_Bytes(CRYPTO0(), ecc_private_key1, sizeof(t_ecc_private_key1), t_ecc_private_key1);
    CRYPTO_SWAP_Bytes(CRYPTO0(), ecc_remote_public_key1,   sizeof(t_ecc_public_key1)/2,  t_ecc_public_key1);
    CRYPTO_SWAP_Bytes(CRYPTO0(), ecc_remote_public_key1+6,   sizeof(t_ecc_public_key1)/2, t_ecc_public_key1+6);

    status = CRYPTO_ECC_Multiply(CRYPTO0(), t_ecc_dh_key1, t_ecc_public_key1, t_ecc_private_key1);

    CRYPTO_SWAP_Bytes(CRYPTO0(), t_ecc_dh_key1,     sizeof(t_ecc_dh_key1)/2,  ecc_dh_key);
    CRYPTO_SWAP_Bytes(CRYPTO0(), t_ecc_dh_key1+6,   sizeof(t_ecc_dh_key1)/2,  ecc_dh_key+6);

    CRYPTO_PowerControl(CRYPTO0(), CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);

    return status;
}

int32_t bt_ble_p256_private_public_key_gen(uint32_t seed, uint32_t* ecc_private_key1, uint32_t*ecc_public_key1)
{
    int32_t status;
    uint32_t t_ecc_private_key1[8];
    uint32_t t_ecc_public_key1[16];
    //CLOGI("seed:0x%x", seed);
    CRYPTO_PowerControl(CRYPTO0(), CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0(), CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P256);
    status = CRYPTO_ECC_Generate_Key(CRYPTO0(), seed, t_ecc_private_key1, t_ecc_public_key1);

    CRYPTO_SWAP_Bytes(CRYPTO0(), t_ecc_private_key1, sizeof(t_ecc_private_key1), ecc_private_key1);
    CRYPTO_SWAP_Bytes(CRYPTO0(), t_ecc_public_key1,  sizeof(t_ecc_public_key1)/2,  ecc_public_key1);
    CRYPTO_SWAP_Bytes(CRYPTO0(), t_ecc_public_key1+8,  sizeof(t_ecc_public_key1)/2,  ecc_public_key1+8);

    CRYPTO_PowerControl(CRYPTO0(), CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);

    return status;
}

int32_t bt_ble_p256_dh_key_gen(uint32_t* ecc_private_key1, uint32_t*ecc_remote_public_key1, uint32_t *ecc_dh_key)
{
    int32_t status;
    uint32_t t_ecc_private_key1[8];
    uint32_t t_ecc_public_key1[16];
    uint32_t t_ecc_dh_key1[16];
    CRYPTO_PowerControl(CRYPTO0(), CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_FULL);
    CRYPTO_Control(CRYPTO0(), CSK_CRYPTO_SET_ECC_CURVE, (uint32_t)&CRYPTO_ECC_CURVE_P256);

    CRYPTO_SWAP_Bytes(CRYPTO0(), ecc_private_key1, sizeof(t_ecc_private_key1), t_ecc_private_key1);
    CRYPTO_SWAP_Bytes(CRYPTO0(), ecc_remote_public_key1,   sizeof(t_ecc_public_key1)/2,  t_ecc_public_key1);
    CRYPTO_SWAP_Bytes(CRYPTO0(), ecc_remote_public_key1+8, sizeof(t_ecc_public_key1)/2, t_ecc_public_key1+8);
    status =  CRYPTO_ECC_Multiply(CRYPTO0(), t_ecc_dh_key1, t_ecc_public_key1, t_ecc_private_key1);

    CRYPTO_SWAP_Bytes(CRYPTO0(), t_ecc_dh_key1,     sizeof(t_ecc_dh_key1)/2,  ecc_dh_key);
    CRYPTO_SWAP_Bytes(CRYPTO0(), t_ecc_dh_key1+8,   sizeof(t_ecc_dh_key1)/2,  ecc_dh_key+8);

    CRYPTO_PowerControl(CRYPTO0(), CSK_CRYPTO_HW_ECC_RSA, CSK_POWER_OFF);

    return status;
}
