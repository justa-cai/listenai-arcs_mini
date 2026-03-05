#ifndef VENDOR_OPS_H
#define VENDOR_OPS_H

#include <stdint.h>

#define VENDOR_WIFI_MAC_ID  0x01
#define VENDOR_TAG          0x4C495341  /* 'LISA' in hex */

#define VENDOR_ITEM_NUM     16

#define VENDOR_INFO_SIZE        CONFIG_VENDOR_STORAGE_SIZE
#define VENDOR_DATA_OFFSET      (sizeof(struct vendor_hdr) + (sizeof(struct vendor_item) * VENDOR_ITEM_NUM))
#define VENDOR_VERSION2_OFFSET  (VENDOR_INFO_SIZE - 4)

struct vendor_hdr {
    uint32_t tag;
    uint32_t version;
    uint16_t part_index;
    uint16_t item_num;
    uint16_t free_offset;
    uint16_t free_size;
};

struct vendor_item {
    uint16_t id;
    uint16_t offset;
    uint16_t size;
    uint16_t reserved;
};

struct vendor_info {
    struct vendor_hdr *hdr;
    struct vendor_item *item;
    uint8_t *data;
    uint32_t *version2;
};

/**
 * Write vendor-specific data to flash storage.
 * @param id The ID of the vendor item.
 * @param pbuf Pointer to the buffer containing the data to write.
 * @param size Size of the data to write.
 * @return 0 on success, -1 on failure.
 */
int vendor_storage_write(uint32_t id, void *pbuf, uint32_t size);

/**
 * Read vendor-specific data from flash storage.
 * @param id The ID of the vendor item.
 * @param pbuf Pointer to the buffer to store the read data.
 * @param size Size of the data to read.
 * @return 0 on success, -1 on failure.
 */
int vendor_storage_read(uint32_t id, void *pbuf, uint32_t size);

#endif /* VENDOR_OPS_H */
