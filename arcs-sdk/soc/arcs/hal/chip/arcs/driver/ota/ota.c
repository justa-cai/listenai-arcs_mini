/**
 ****************************************************************************************
 *
 * @file ota.c
 *
 * @brief OTA common function implement.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "arcs_ap.h"
#include "cache.h"
#include "ota.h"
#include "spiflash.h"
#include "Driver_CRYPTO.h"

/// ota state
enum {
    OTA_IDLE = 0,
    /// version check ok
    OTA_VERSION_CHECK,
    /// during data write
    OTA_DATA_WRITE,
    /// data verify success
    OTA_DATA_VERIFY,

};

typedef struct {
    /// ota state
    uint8_t      state;
    /// flag of the ota file
    uint8_t      new_flag;
    /// new version
    ls_ota_ver_t new_ver;
    /// ota write base offset
    uint32_t     base;
    /// ota zone size
    uint32_t     size;
    /// ota configure
    ls_ota_config_t *pConfig;
    /// flash device
    FLASH_DEV   *flash_dev;
    /// flash base address
    uint32_t    flash_base;
    /// crypto handler
    void *crypto_handler;
} ls_ota_env_t;


static ls_ota_env_t ota_env;

/* Unique initialisation vector */
static const unsigned char ota_aes_cbc_iv[] = {
    0x43, 0x68, 0x6d, 0x6a, 0x89, 0x65, 0xb2, 0x95, 0xf7, 0x2d, 0xc7, 0x3f, 0xe0, 0x3d, 0x97, 0x3d,
};


extern uint32_t crc32(uint32_t val, const uint8_t *buf, size_t len);


uint8_t ota_check_sum(ls_ota_header_t *hdr)
{
    if(hdr == NULL)
        return 0;

    if(hdr->valid_flag != OTA_EXEC_VALID_FLAG)
        return 0;

    uint32_t checksum = 0;
    int count = hdr->size / 1024;

    if(count > 32)
        count = 32;

    uint32_t *pdu = (uint32_t*)hdr;
    while(count--)
    {
        pdu += 0x100; // skip 1KB
        checksum ^= (*pdu);
    };

    return checksum==hdr->checksum;
}

uint8_t ota_check_zone_crc(ls_ota_header_t *hdr)
{
    uint32_t crc = 0;
    uint32_t flags = -1;

    if(hdr == NULL)
        return 0;

    ls_ota_header_t hdr_orgin;
    hdr_orgin = *hdr;
    /// the crc is based on the flag 0xffffffff
    hdr_orgin.valid_flag = 0xffffffff;
    /// the crc32 is base on zeros with crc value
    hdr_orgin.crc32 = 0;
    /// remove enc flag
    hdr_orgin.flags &= ~(OTA_ENC_MASK);

    /// calculate header
    crc = crc32(crc, (const uint8_t*)&hdr_orgin, sizeof(ls_ota_header_t));

    /// calculate the image data crc
    crc = crc32(crc, (const uint8_t*)(hdr + 1), hdr->size - sizeof(ls_ota_header_t));

    return crc==hdr->crc32;
}

uint8_t ota_initialize(FLASH_DEV *flash_dev)
{
    uint32_t flash_base;
    ls_ota_header_t *boot_loader;
#if FLASH_ENC_ALL
    flash_base = CP_CIPHER_REGION_A;
#else
    flash_base = CMN_FLASH_REGION;
#endif
    boot_loader = (ls_ota_header_t *)flash_base;
    IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_TGT_SLV_SEL = 1; // map address to flash
    IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_A = 1; // enable region A encrypt

    if(!ota_check_sum(boot_loader))
    {
        return OTA_INVALID_VERSION;
    }
    ota_env.flash_dev = flash_dev;
    ota_env.pConfig = (ls_ota_config_t *)boot_loader->reserved[3]; //use reserved[3] to save ota config address

    if(ota_env.pConfig != NULL)
        return OTA_SUCCESS;
    else
        return OTA_INVALID_VERSION;
}

uint8_t ota_uninitialize()
{
    IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_A = 0;
    return OTA_SUCCESS;
}

/// get the image version with given zone_id
const ls_ota_ver_t *ota_get_current_version(uint8_t zone_id)
{
    if(ota_env.pConfig == NULL)
        return NULL;

    ls_ota_header_t *zone = ota_find_zone(zone_id);

    if(zone == NULL)
        return NULL;

    return &(zone->version);
}

uint8_t ota_find_zone_table(uint8_t zone_id)
{
    uint8_t idx=0xff;

    if(ota_env.pConfig == NULL)
        return idx;

    for(idx=0;idx<ota_env.pConfig->zone_count;idx++)
    {
        if(zone_id == ota_env.pConfig->zones[idx].id)
        {
            return idx;
        }
    }
    return idx;
}

ls_ota_header_t *ota_find_zone(uint8_t id)
{
    ls_ota_header_t *result = NULL;
    ls_ota_header_t *hdr;
    ls_ota_zone_t *zones = ota_env.pConfig->zones;
    uint32_t flash_base;

    if(ota_env.pConfig == NULL)
        return NULL;

    uint8_t idx = ota_find_zone_table(id);
    if(idx == 0xff)
        return NULL;

    if(ota_env.pConfig->zones[idx].enc)
        flash_base = CP_CIPHER_REGION_A;
    else
        flash_base = CMN_FLASH_REGION;

    for(int i=0; i<ota_env.pConfig->zone_count; i++)
    {
        hdr = (ls_ota_header_t*)(flash_base+zones[i].address);
        if(hdr->version.zone_id == id && ota_check_sum(hdr))
        {
            if(result == NULL || hdr->version.version > result->version.version)
                result = hdr;
        }
    }

    return result;
}

static uint8_t ota_find_blank_zone(uint32_t size)
{
    uint8_t res = 0xff;
    ls_ota_header_t *hdr, *hdr2;
    ls_ota_zone_t *zones = ota_env.pConfig->zones;
    int i,j;

    for(i=0; i<ota_env.pConfig->zone_count; i++)
    {
        if(zones[i].id == 0 || zones[i].id > 0xf0) // not execute zone
        {
            continue;
        }
        hdr = (ls_ota_header_t*)(CMN_FLASH_REGION+zones[i].address);
        if(!ota_check_sum(hdr))
        {
            hdr = (ls_ota_header_t*)(CP_CIPHER_REGION_A+zones[i].address);
        }
        if(!ota_check_sum(hdr))  // invalid execute zone
        {
            if(zones[i].size >= size)
            {
                res = i;
                break;
            }
            else
            {
                continue;
            }
        }
        else
        {
            for(j=i+1; j<ota_env.pConfig->zone_count; j++)
            {
                hdr2 = (ls_ota_header_t*)(CMN_FLASH_REGION+zones[j].address);
                if(!ota_check_sum(hdr2))
                {
                    hdr2 = (ls_ota_header_t*)(CP_CIPHER_REGION_A+zones[j].address);
                }

                if(ota_check_sum(hdr2) && hdr->version.zone_id == hdr2->version.zone_id)
                {
                    if(hdr->version.version > hdr2->version.version)
                        res = j;
                    else
                        res = i;
                    if(zones[res].size >= size)
                        break;
                    else
                        res = 0xff; // zone size not enough
                }
            }
            if(res < ota_env.pConfig->zone_count && zones[res].size >= size)
                break;
        }
    }

    return res;
}

static uint8_t ota_check_version(ls_ota_ver_t *new_ver)
{
    const ls_ota_ver_t *old_ver = ota_get_current_version(new_ver->zone_id);

    uint8_t result = OTA_INVALID_VERSION;

    if(ota_env.pConfig == NULL)
        return OTA_INVALID_PARAM;

    ota_env.state = OTA_IDLE;
    do
    {
        /// check id
        if(new_ver->vendor_id != ota_env.pConfig->vendor_id)
            break;
        if(new_ver->device_id != ota_env.pConfig->device_id)
            break;

        /// check version
        if(ota_env.pConfig->ota_mode == OTA_MODE_OVERWRITE)
        {
            if(new_ver->version == old_ver->version)
                break;
        }
        else
        {
            if(new_ver->version <= old_ver->version)
                break;
        }


        result = OTA_VERSION_CONFIRM;
        ota_env.state = OTA_VERSION_CHECK;
        ota_env.new_ver = *new_ver;
    }while(0);

    return result;
}

static uint8_t ota_start_ota(ls_ota_start_cmd_t* cmd)
{
    uint8_t result = OTA_SUCCESS;
    uint8_t zone_id = ota_env.new_ver.zone_id;
    const ls_ota_ver_t *old_ver = ota_get_current_version(zone_id);
    uint32_t aes_length[4] = {0};
    int i;

    ota_env.base = 0;

    if(ota_env.pConfig == NULL)
        return OTA_INVALID_PARAM;

    uint8_t idx = ota_find_zone_table(zone_id);
    if(idx == 0xff)
    {
        return OTA_INVALID_VERSION;
    }
    if(ota_env.pConfig->zones[idx].enc)
        ota_env.flash_base = CP_CIPHER_REGION_A;
    else
        ota_env.flash_base = CMN_FLASH_REGION;


    do
    {
        if(ota_env.state != OTA_VERSION_CHECK)
        {
            result = OTA_INVALID_VERSION;
            break;
        }

        if(ota_env.pConfig->ota_mode == OTA_MODE_OVERWRITE)
        {
            if(old_ver->zone_id != zone_id)
            {
                result = OTA_INVALID_VERSION;
                break;
            }
            /// write data to OTA zone
            for(i=0; i<ota_env.pConfig->zone_count; i++)
            {
                if(ota_env.pConfig->zones[i].id == OTA_ZONE_ID_OTA)
                {
                    if(cmd->size <= ota_env.pConfig->zones[i].size)
                        ota_env.base = ota_env.pConfig->zones[i].address;
                }
            }
            if(ota_env.base == 0)
            {
                result = OTA_INVALID_PARAM;
                break;
            }
        }
        else /// switch mode
        {
            i = ota_find_blank_zone(cmd->size);
            if(i >= ota_env.pConfig->zone_count)
            {
                result = OTA_INVALID_VERSION;
                break;
            }
            if(cmd->size <= ota_env.pConfig->zones[i].size)
                ota_env.base = ota_env.pConfig->zones[i].address;
            else
            {
                result = OTA_INVALID_PARAM;
                break;
            }
        }

        ota_env.new_flag = cmd->flags;
        ota_env.size = cmd->size;
        ota_env.crypto_handler = cmd->crypto_handler;

        // prepare aes enc
        if(ota_env.new_flag & OTA_ENC_MASK)
        {
            aes_length[3] = cmd->size - sizeof(ls_ota_header_t);
            CRYPTO_PowerControl(ota_env.crypto_handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_FULL);

            CRYPTO_Control(ota_env.crypto_handler,CSK_CRYPTO_SET_AES_MODE, CSK_CRYPTO_AES_MODE_CBC);
            CRYPTO_Control(ota_env.crypto_handler, CSK_CRYPTO_SET_AES_KEY_SIZE_256, 0);
            CRYPTO_Control(ota_env.crypto_handler, CSK_CRYPTO_AES_KEY_MODE_EFUSE1, 0);
            CRYPTO_Control(ota_env.crypto_handler, CSK_CRYPTO_SET_AES_IV, (uint32_t)ota_aes_cbc_iv);
            CRYPTO_Control(ota_env.crypto_handler, CSK_CRYPTO_SET_AES_LENGTHS, (uint32_t)aes_length);
        }

        if(!ota_env.pConfig->zones[idx].enc)
            IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_A = 0; // disable flash write encrypt

        flash_write_protection_set(ota_env.flash_dev, false);

        /// earse flash
        result = flash_erase(ota_env.flash_dev, ota_env.base, cmd->size);
        if(result != 0)
            result = OTA_FLASH_ERROR;

        flash_write_protection_set(ota_env.flash_dev, true);

    }while(0);

    if(result == OTA_SUCCESS)
    {
        ota_env.state = OTA_DATA_WRITE;
        result = OTA_START_CONFIRM;
    }
    else
        ota_env.state = OTA_IDLE;

    return result;
}

static uint8_t ota_write_data(ls_ota_data_t *cmd)
{
    uint8_t result = OTA_SUCCESS;
    if(ota_env.pConfig == NULL)
        return OTA_INVALID_PARAM;

    do
    {
        if(ota_env.state != OTA_DATA_WRITE)
        {
            result = OTA_INVALID_CMD;
            break;
        }

        if(cmd->address > ota_env.size)
        {
            result = OTA_INVALID_PARAM;
            break;
        }

        // decrypt if need
        if(ota_env.new_flag & OTA_ENC_MASK)
        {
            if(cmd->address == 0) // skip header
                CRYPTO_AES_Decrypt(ota_env.crypto_handler, (uint32_t*)(cmd->data + sizeof(ls_ota_header_t)), cmd->length - sizeof(ls_ota_header_t), (uint32_t*)(cmd->data + sizeof(ls_ota_header_t)));
            else
                CRYPTO_AES_Decrypt(ota_env.crypto_handler, (uint32_t*)cmd->data, cmd->length, (uint32_t*)cmd->data);
        }
        // remove encrypt padding if needed
        if(cmd->address + cmd->length > ota_env.size)
        {
            cmd->length = ota_env.size - cmd->address;
        }

        flash_write_protection_set(ota_env.flash_dev, false);
        /// write flash
        result = flash_write(ota_env.flash_dev, ota_env.base + cmd->address, cmd->data, cmd->length);
        if(result != 0)
            result = OTA_FLASH_ERROR;

        flash_write_protection_set(ota_env.flash_dev, true);

        /// check sum
        if(cmd->crc32 !=0 && crc32(0, (uint8_t *)ota_env.flash_base + ota_env.base + cmd->address, cmd->length)
                  != cmd->crc32){
        	result = OTA_VERIFY_ERROR;
        }

    }while(0);

    if(result == OTA_SUCCESS)
    {
        result = OTA_DATA_CONFIRM;
    }
    return result;
}


static uint8_t ota_check_data(ls_ota_verify_cmd_t* cmd)
{
    uint8_t result = OTA_SUCCESS;

    if(ota_env.state != OTA_DATA_WRITE)
    {
        return OTA_INVALID_CMD;
    }
    if(ota_env.pConfig == NULL)
        return OTA_INVALID_PARAM;

    HAL_InvalidateDCache();

    if(ota_check_zone_crc((ls_ota_header_t *)(ota_env.flash_base + ota_env.base)))
    {
        uint8_t idx = ota_find_zone_table(ota_env.new_ver.zone_id);
        if(ota_env.pConfig->zones[idx].sign_mode > OTA_SIGN_CRC32)
        {
            if(CSK_DRIVER_OK != CRYPTO_Verify_Flash_Signature(ota_env.crypto_handler, (uint8_t*)(ota_env.flash_base + ota_env.base), ota_env.pConfig->zones[idx].sign_mode))
            {
                return OTA_VERIFY_ERROR;
            }
        }

        /// write zone valid flag
        uint32_t flag;
        if(ota_env.pConfig->ota_mode == OTA_MODE_OVERWRITE)
            flag = OTA_OTA_VALID_FLAG;
        else /// switch mode
            flag = OTA_EXEC_VALID_FLAG;

        flash_write_protection_set(ota_env.flash_dev, false);
        /// write flash
        result = flash_write(ota_env.flash_dev, ota_env.base, &flag, 4);
        if(result != 0)
            result = OTA_FLASH_ERROR;
        else
            result = OTA_VERIFY_CONFIRM;

        flash_write_protection_set(ota_env.flash_dev, true);
    }
    else
    {
        result = OTA_VERIFY_ERROR;
    }

    // close aes
    if(ota_env.new_flag & OTA_ENC_MASK)
    {
        CRYPTO_PowerControl(ota_env.crypto_handler, CSK_CRYPTO_HW_AES_SHA, CSK_POWER_OFF);
    }

    ota_env.state = OTA_DATA_VERIFY;

    return result;
}

uint8_t ota_process_command(ls_ota_cmd_t *cmd)
{
    uint8_t result = OTA_SUCCESS;
    if(ota_env.pConfig == NULL)
        return OTA_INVALID_PARAM;

    //if(offset == 0)
    {
        switch(cmd->opcode)
        {
        case OTA_NEW_VERSION:
            result = ota_check_version((ls_ota_ver_t*)&(cmd->version));
            break;
        case OTA_OTA_START:
            result = ota_start_ota((ls_ota_start_cmd_t*)&(cmd->start));
            break;
        case OTA_WRITE_DATA:
            result = ota_write_data((ls_ota_data_t*)&(cmd->data));
            break;
        case OTA_OTA_VERIFY:
            result = ota_check_data((ls_ota_verify_cmd_t*)&(cmd->verify));
            break;

        default:
            result = OTA_INVALID_CMD;
            break;
        }
    }

    return result;
}
