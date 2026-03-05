/**
    * @file vendor_ops.c
    * @brief Vendor storage operations using LISA Flash
    * This file provides functions to read and write vendor-specific data
    * to a designated area in the flash memory.
    TODO: Add dual parts backup to avoid data loss during power failure.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_flash.h"
#include "vendor_ops.h"
#ifdef CONFIG_SDK_MODULE_LETTER_SHELL
#include "shell.h"
#endif

#define TAG "vendor_ops"
#include "lisa_log.h"

static struct vendor_info vendor_info;

int vendor_storage_write(uint32_t id, void *pbuf, uint32_t size)
{
    lisa_device_t *flash = lisa_device_get("flash0");
    if (!flash || !lisa_device_ready(flash)) {
        LOGE("Flash device not ready");
        return -1;
    }

    uint8_t *vendor_buf = (uint8_t *)malloc((uint32_t)VENDOR_INFO_SIZE);

    if (!vendor_buf) {
        LOGE("Failed to allocate vendor storage buffer");
        return -1;
    }

    vendor_info.hdr = (struct vendor_hdr *)vendor_buf;
    vendor_info.item = (struct vendor_item *)(vendor_buf + sizeof(struct vendor_hdr));
    vendor_info.data = vendor_buf + VENDOR_DATA_OFFSET;
    vendor_info.version2 = (uint32_t *)(vendor_buf + VENDOR_VERSION2_OFFSET);

    if (lisa_flash_read(flash, CONFIG_VENDOR_STORAGE_OFFSET, vendor_buf, (uint32_t)VENDOR_INFO_SIZE) < 0) {
        LOGE("Failed to read vendor storage from flash");
        free(vendor_buf);
        return -1;
    }

    if (vendor_info.hdr->tag == VENDOR_TAG) {
        // find item
        int i;
        for (i = 0; i < vendor_info.hdr->item_num; i++) {
            if (vendor_info.item[i].id == id) {
                break;
            }
        }

        if (i == vendor_info.hdr->item_num) {
            // new item
            if (vendor_info.hdr->item_num >= VENDOR_ITEM_NUM) {
                LOGE("Vendor storage full");
                free(vendor_buf);
                return -1;
            }
            // add new item
            vendor_info.item[i].id = id;
            vendor_info.item[i].offset = VENDOR_DATA_OFFSET + ((uint32_t)VENDOR_INFO_SIZE - VENDOR_DATA_OFFSET - vendor_info.hdr->free_size);
            vendor_info.item[i].size = size;
            vendor_info.hdr->item_num++;
            vendor_info.hdr->free_offset += size;
            vendor_info.hdr->free_size -= size;
        }
        else {
            // existing item, check size
            if (vendor_info.item[i].size < size) {
                LOGE("Existing item size too small");
                free(vendor_buf);
                return -1;
            }
        }

        // copy data
        memcpy(vendor_buf + vendor_info.item[i].offset, pbuf, size);

    }
    else {
        // init vendor storage
        memset(vendor_buf, 0xFF, (uint32_t)VENDOR_INFO_SIZE);
        vendor_info.hdr->tag = VENDOR_TAG;
        vendor_info.hdr->version = 1;
        vendor_info.hdr->item_num = 0;
        vendor_info.hdr->free_offset = 0;
        vendor_info.hdr->free_size = (uint32_t)VENDOR_INFO_SIZE - sizeof(struct vendor_hdr) - (sizeof(struct vendor_item) * VENDOR_ITEM_NUM) - 4;

        // add new item
        vendor_info.item[0].id = id;
        vendor_info.item[0].offset = VENDOR_DATA_OFFSET;
        vendor_info.item[0].size = size;
        vendor_info.hdr->item_num = 1;
        vendor_info.hdr->free_offset += size;
        vendor_info.hdr->free_size -= size;

        // copy data
        memcpy(vendor_buf + vendor_info.item[0].offset, pbuf, size);
    }

    // write back to flash
    if (lisa_flash_erase(flash, CONFIG_VENDOR_STORAGE_OFFSET, (uint32_t)VENDOR_INFO_SIZE) < 0 ||
        lisa_flash_write(flash, CONFIG_VENDOR_STORAGE_OFFSET, vendor_buf, (uint32_t)VENDOR_INFO_SIZE) < 0) {
        LOGE("Failed to write vendor storage to flash");
        free(vendor_buf);
        return -1;
    }

    return 0;
}

int vendor_storage_read(uint32_t id, void *pbuf, uint32_t size)
{
    lisa_device_t *flash = lisa_device_get("flash0");
    if (!flash || !lisa_device_ready(flash)) {
        LOGE("Flash device not ready");
        return -1;
    }

    uint8_t *vendor_buf = (uint8_t *)malloc((uint32_t)VENDOR_INFO_SIZE);

    if (!vendor_buf) {
        LOGE("Failed to allocate vendor storage buffer");
        return -1;
    }

    vendor_info.hdr = (struct vendor_hdr *)vendor_buf;
    vendor_info.item = (struct vendor_item *)(vendor_buf + sizeof(struct vendor_hdr));
    vendor_info.data = vendor_buf + VENDOR_DATA_OFFSET;
    vendor_info.version2 = (uint32_t *)(vendor_buf + VENDOR_VERSION2_OFFSET);

    if (lisa_flash_read(flash, CONFIG_VENDOR_STORAGE_OFFSET, vendor_buf, (uint32_t)VENDOR_INFO_SIZE) < 0) {
        LOGE("Failed to read vendor storage from flash");
        free(vendor_buf);
        return -1;
    }

    if (vendor_info.hdr->tag != VENDOR_TAG) {
        LOGE("Invalid vendor storage tag");
        free(vendor_buf);
        return -1;
    }

    // find item
    int i;
    for (i = 0; i < vendor_info.hdr->item_num; i++) {
        if (vendor_info.item[i].id == id) {
            break;
        }
    }

    if (i == vendor_info.hdr->item_num) {
        LOGE("Vendor item not found");
        free(vendor_buf);
        return -1;
    }

    if (vendor_info.item[i].size < size) {
        LOGE("Vendor item size too small");
        free(vendor_buf);
        return -1;
    }

    // copy data
    memcpy(pbuf, vendor_buf + vendor_info.item[i].offset, size);

    free(vendor_buf);
    return 0;
}

int vendor_storage_write_mac(const char *mac_str)
{
    uint8_t mac_addr[6];
    
    if (!mac_str) {
        LOGE("Invalid MAC address string");
        return -1;
    }
    
    if (sscanf(mac_str, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
               &mac_addr[0], &mac_addr[1], &mac_addr[2],
               &mac_addr[3], &mac_addr[4], &mac_addr[5]) != 6) {
        LOGE("Invalid MAC address format");
        return -1;
    }
    
    return vendor_storage_write(VENDOR_WIFI_MAC_ID, mac_addr, sizeof(mac_addr));
}

int vendor_storage_read_mac(uint8_t *mac_addr)
{
    if (!mac_addr) {
        LOGE("Invalid MAC address buffer");
        return -1;
    }
    
    return vendor_storage_read(VENDOR_WIFI_MAC_ID, mac_addr, 6);
}

#ifdef CONFIG_SDK_MODULE_LETTER_SHELL
static int cmd_vendor_read_mac(int argc, char **argv)
{
    uint8_t mac_addr[6];
    
    if (vendor_storage_read_mac(mac_addr) < 0) {
        LOGE("Failed to read MAC address");
        return -1;
    }
    
    LOGI("MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
         mac_addr[0], mac_addr[1], mac_addr[2],
         mac_addr[3], mac_addr[4], mac_addr[5]);
    return 0;
}

static int cmd_vendor_write_mac(int argc, char **argv)
{
    if (argc < 2) {
        LOGE("Usage: vendor_write_mac <mac_address>");
        return -1;
    }
    
    return vendor_storage_write_mac(argv[1]);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 vendor_mac_write, cmd_vendor_write_mac, "Write wifi mac address");
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 vendor_mac_read, cmd_vendor_read_mac, "Read wifi mac address");
#endif