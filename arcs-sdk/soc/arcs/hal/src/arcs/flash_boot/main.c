/**
****************************************************************************************
*
* @file main.c
*
* @brief Flash boot main source
*
* Copyright (C) ListenAI 2020-2099
*
*
****************************************************************************************
*/

#include "stdint.h"
#include "string.h"
#include "stdbool.h"
#include "chip.h"
#include "spiflash.h"
#include "ota_config.h"
#include "ota.h"
#include "Driver_CRYPTO.h"
#include "secure.h"
#include "cache.h"


typedef void (*func_entry) (void);

extern void* CRYPTO0_Handler;
const ls_ota_config_t ota_config;
static uint32_t ota_buff[OTA_BLOCK_SIZE/4];
static FLASH_DEV flash_boot_dev = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF, //divider is 1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000,
    .addr_bytes = 3,
    .addr_auto = 0,
};

static void flash_boot_copy_ramtxt()
{
    extern uint32_t _text_lma, _ram_code_start, _ram_code_end,_rom_code_start,_rom_code_end;
    uint8_t *lma, *vma;
    uint32_t i, size;

    /// copy ram text seg
    lma = (uint8_t *)&_text_lma;
    vma = (uint8_t *)&_ram_code_start;
    size = (uint32_t)(&_ram_code_end) - (uint32_t)(&_ram_code_start);
    for(i = 0; i < size; i++) {
        vma[i] = lma[i];
    }
}

static void ota_copy_flash(FLASH_DEV *dev, void *src, uint32_t dest_addr, int32_t size)
{
    int ret;

    while(size > 0)
    {
        ret = flash_erase(dev, dest_addr, OTA_BLOCK_SIZE);
        if(ret)
            break;
        memcpy(ota_buff, src, OTA_BLOCK_SIZE);
        ret = flash_write(dev, dest_addr, ota_buff, OTA_BLOCK_SIZE);
        if(ret)
            break;

        src += OTA_BLOCK_SIZE;
        dest_addr += OTA_BLOCK_SIZE;
        size -= OTA_BLOCK_SIZE;
    };
}

int main()
{
    int idx;
    uint32_t valid_flag;

    CMN_SYSCFG_RegDef *g_sysctrl = IP_SYSCTRL;
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_TGT_SLV_SEL = 1; // map address to flash
    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_A = 1; // enable region A encrypt

    flash_init(&flash_boot_dev, 0, 0);
    ota_initialize(&flash_boot_dev);

    // for overwrite ota mode, copy ota data if needed
    if(ota_config.ota_mode == OTA_MODE_OVERWRITE)
    {
        ls_ota_header_t *ota_header = (ls_ota_header_t*)(CMN_FLASH_REGION+OTA_ZONE_OTA_ADDRESS);

        if(ota_header->valid_flag != OTA_OTA_VALID_FLAG)
        {
            ota_header = (ls_ota_header_t*)(CP_CIPHER_REGION_A+OTA_ZONE_OTA_ADDRESS);
        }
        /// check copy ota needed
        if(ota_header->valid_flag == OTA_OTA_VALID_FLAG)
        {
            ls_ota_header_t *exec_header = NULL;

            idx = ota_find_zone_table(ota_header->version.zone_id);
            if(idx < ota_config.zone_count)
            {
                if(ota_config.zones[idx].enc)
                    exec_header = (ls_ota_header_t *)(CP_CIPHER_REGION_A+ota_config.zones[idx].address);
                else
                {
                    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_A = 0; // disable flash encrypt
                    exec_header = (ls_ota_header_t *)(CMN_FLASH_REGION+ota_config.zones[idx].address);
                }
            }

            do
            {
                if(exec_header == NULL)
                    break;

                /// check ota version
                if(exec_header->valid_flag == OTA_EXEC_VALID_FLAG && ota_header->version.version == exec_header->version.version)
                {
                    break;
                }

                /// check ota id
                if(ota_header->version.vendor_id != OTA_VENDOR_ID || ota_header->version.device_id != OTA_DEVICE_ID)
                {
                    break;
                }

                /// check ota data
                if(ota_check_zone_crc(ota_header))
                {
                    if(ota_config.zones[idx].sign_mode > OTA_SIGN_CRC32)
                    {
                        // check signature
                        secure_init();
                        if(CSK_DRIVER_OK != CRYPTO_Verify_Flash_Signature(CRYPTO0_Handler, ota_header, ota_config.zones[idx].sign_mode))
                        {
                            secure_shutdown();
                            flash_write_protection_set(&flash_boot_dev, false);
                            /// clear ota data flag
                            valid_flag = 0;
                            flash_write(&flash_boot_dev, OTA_ZONE_OTA_ADDRESS, &valid_flag, 4);
                            flash_write_protection_set(&flash_boot_dev, true);
                            break;
                        }
                        secure_shutdown();
                    }

                    flash_write_protection_set(&flash_boot_dev, false);

                    /// start flash copy
                    ota_copy_flash(&flash_boot_dev, ota_header, ota_config.zones[idx].address, ota_header->size);
                    flash_write_protection_set(&flash_boot_dev, true);
                    HAL_InvalidateDCache();
                    //HAL_InvalidateDCache_by_Addr(exec_header, ota_header->size);
                    /// check copy data
                    if(ota_check_zone_crc(exec_header))
                    {
                        valid_flag = OTA_EXEC_VALID_FLAG;
                        flash_write_protection_set(&flash_boot_dev, false);
                        /// update execute data flag
                        flash_write(&flash_boot_dev, ota_config.zones[idx].address, &valid_flag, 4);
                        /// clear ota data flag
                        valid_flag = 0;
                        flash_write(&flash_boot_dev, OTA_ZONE_OTA_ADDRESS, &valid_flag, 4);
                        flash_write_protection_set(&flash_boot_dev, true);
                        HAL_InvalidateDCache();
                    }
                }
            }
            while(0);
        }
    }

    /// map user zone
    {
        g_sysctrl->REG_CIPHER_CTRL1.bit.CIPHER_DEV_OFFSET_REGION_D = (OTA_ZONE_USER_ADDRESS)/4096;
        g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_D = OTA_ZONE_USER_ENC;
    }

    uint32_t cp_entry = 0;
    func_entry ap_entry = NULL;
    /// find cp exec zone to start cp
    {
        ls_ota_header_t *cp_header = ota_find_zone(OTA_ZONE_ID_CP);

        if(cp_header!= NULL && cp_header->valid_flag == OTA_EXEC_VALID_FLAG && cp_header->version.zone_id == OTA_ZONE_ID_CP
                && cp_header->version.vendor_id == OTA_VENDOR_ID && cp_header->version.device_id == OTA_DEVICE_ID)
        {
            idx = ota_find_zone_table(OTA_ZONE_ID_CP);

            // check signature
            if(ota_config.zones[idx].sign_mode == OTA_SIGN_CRC32)
            {
                if(!ota_check_zone_crc(cp_header))
                    goto error;
            }
            else if(ota_config.zones[idx].sign_mode > OTA_SIGN_CRC32)
            {
                secure_init();
                if(CSK_DRIVER_OK != CRYPTO_Verify_Flash_Signature(CRYPTO0_Handler, cp_header, ota_config.zones[idx].sign_mode))
                {
                    secure_shutdown();
                    goto error;
                }
                secure_shutdown();
            }

            // map cp image address
            g_sysctrl->REG_CIPHER_CTRL2.bit.CIPHER_DEV_OFFSET_REGION_C = (((uint32_t)cp_header)/4096)&0x7fff;
            g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_C = ota_config.zones[idx].enc;

            cp_entry = cp_header->entry;
        }
    }

    /// find newer exec zone to jump
    {
        ls_ota_header_t *ap_header = ota_find_zone(OTA_ZONE_ID_AP);

        if(ap_header!= NULL && ap_header->valid_flag == OTA_EXEC_VALID_FLAG && ap_header->version.zone_id == OTA_ZONE_ID_AP
                && ap_header->version.vendor_id == OTA_VENDOR_ID && ap_header->version.device_id == OTA_DEVICE_ID)
        {
            idx = ota_find_zone_table(OTA_ZONE_ID_AP);

            // check signature
            if(ota_config.zones[idx].sign_mode == OTA_SIGN_CRC32)
            {
                if(!ota_check_zone_crc(ap_header))
                    goto error;
            }
            else if(ota_config.zones[idx].sign_mode > OTA_SIGN_CRC32)
            {
                secure_init();
                if(CSK_DRIVER_OK != CRYPTO_Verify_Flash_Signature(CRYPTO0_Handler, ap_header, ota_config.zones[idx].sign_mode))
                {
                    secure_shutdown();
                    goto error;
                }
                secure_shutdown();
            }

            // map ap image address
            g_sysctrl->REG_CIPHER_CTRL2.bit.CIPHER_DEV_OFFSET_REGION_B = (((uint32_t)ap_header)/4096)&0x7fff;
            g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_B = ota_config.zones[idx].enc;

            ap_entry = (func_entry)ap_header->entry;

        }
    }
    disable_GINT();

    g_sysctrl->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_A = 0; // disable flash encrypt

    // start cp
    if(cp_entry != 0)
    {
        IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = cp_entry;
        g_sysctrl->REG_SW_RESET_CP0.all = 0xCAFE000A;
    }
    // start ap
    if(ap_entry != NULL)
    {
        ap_entry();
    }

error:
    while(1);

    return 0;
}

extern void _start();

OTA_HEADER const ls_ota_header_t flash_boot_header = {
        .valid_flag = 0xffffffff,
        .version = {.vendor_id = OTA_VENDOR_ID,
                    .device_id = OTA_DEVICE_ID,
                    .flash_id  = 1,
                    .zone_id   = OTA_ZONE_ID_BOOT,
                    .rom_ver   = 0x0100,
                    .version   = 0x01010001,
                    .date      = BUILD_DATE},
        .flags   = OTA_MODE,
        .address = (uint32_t)&flash_boot_header,
        .entry   = (uint32_t)_start,
        .reserved[0] = 0xffffffff, // compitable with arcs D rom version 0, remove later
        .reserved[3] = (uint32_t)&ota_config, // use reserved[3] to save ota config address
};


/// sign data size: CRC32 - 4, SHA256 - 32, ECSDA256 - 128, RSA2048 - 512
SIGN_DATA const uint32_t sign_data[512/4] =
{
        0
};

#define OTA_ZONE_DESC(zone)  { OTA_ZONE_ID_##zone,OTA_ZONE_##zone##_ENC, OTA_ZONE_##zone##_SIGN, OTA_ZONE_##zone##_ADDRESS, OTA_ZONE_##zone##_SIZE}

const ls_ota_zone_t flash_zone_table[] __attribute__((section(".ROM_DATA"))) = {
        OTA_ZONE_DESC(BOOT), OTA_ZONE_DESC(AP), OTA_ZONE_DESC(CP), OTA_ZONE_DESC(OTA), OTA_ZONE_DESC(USER), OTA_ZONE_DESC(FACT)
};

const ls_ota_config_t ota_config __attribute__((section(".ROM_DATA"))) = {
        /// vendor id
        .vendor_id = OTA_VENDOR_ID,
        /// device id
        .device_id = OTA_DEVICE_ID,
        /// flash base address
#if FLASH_ENC_ALL
        .flash_base = CP_CIPHER_REGION_A,
#else
        .flash_base = CMN_FLASH_REGION,
#endif
        /// OTA mode: 0 - Overwrite mode, 1 - switch mode
        .ota_mode = OTA_MODE,
        /// zone count
        .zone_count = sizeof(flash_zone_table)/sizeof(flash_zone_table[0]),
        .zones = (uint32_t)flash_zone_table,
};
