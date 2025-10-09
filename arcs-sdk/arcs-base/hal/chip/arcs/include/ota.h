/**
 ****************************************************************************************
 *
 * @file ota.h
 *
 * @brief OTA data type definition and function declaration.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef INCLUDE_OTA_OTA_H_
#define INCLUDE_OTA_OTA_H_

#if OTA_ZONE_USER_ADDRESS<OTA_ZONE_OTA_ADDRESS+OTA_ZONE_OTA_SIZE
#error flash zone size overflow!
#endif


#define OTA_HEADER        __attribute__((section(".OTA_HEADER")))
#define SIGN_DATA         __attribute__((section(".SIGN_DATA")))

/// ota zone flag
#define OTA_OTA_VALID_FLAG      0x55aa33cc
#define OTA_EXEC_VALID_FLAG     0x44221188

/// ota zone id define
#define OTA_ZONE_ID_BOOT        0x00
#define OTA_ZONE_ID_AP          0x01
#define OTA_ZONE_ID_CP          0x02
#define OTA_ZONE_ID_EXEC1       0x01
#define OTA_ZONE_ID_EXEC2       0x02
#define OTA_ZONE_ID_EXEC3       0x03
#define OTA_ZONE_ID_OTA         0xf0
#define OTA_ZONE_ID_USER        0xfe
#define OTA_ZONE_ID_FACT        0xff

#define OTA_BLOCK_SIZE      (0x1000)

/// ota status
enum {
   OTA_SUCCESS          = 0,
   OTA_VERSION_CONFIRM  = 1,
   OTA_START_CONFIRM    = 2,
   OTA_DATA_CONFIRM     = 3,
   OTA_VERIFY_CONFIRM   = 4,
   OTA_INVALID_VERSION  = 5,
   OTA_INVALID_CMD      = 6,
   OTA_INVALID_PARAM    = 7,
   OTA_DATA_ERROR       = 8,
   OTA_FLASH_ERROR      = 9,
   OTA_VERIFY_ERROR     = 10,
};

/// ota command
enum {
    /// version of new image
    OTA_NEW_VERSION = 1,
    /// reboot chip
    OTA_REBOOT,

    /// start transport new image
    OTA_OTA_START   = 10,
    /// new image data package
    OTA_WRITE_DATA,
    /// new image verify information
    OTA_OTA_VERIFY,

    /// update device name
    OTA_UPDATE_NAME = 20,
    /// update bt device address
    OTA_UPDATE_BDADDR,

};

/// ota flags
enum {
    /// flag mask
    OTA_MODE_MASK      = (1<<0),
    OTA_ZIP_MASK       = (1<<1),
    OTA_HASH_MASK      = (1<<2),
    OTA_SIGN_MASK      = (1<<3),
    OTA_ENC_MASK       = (1<<4),
};

/// ota mode
enum{
    /// ota mode define
    OTA_MODE_OVERWRITE = 0,
    OTA_MODE_SWITCH    = 1,
    /// sign mode define
    OTA_SIGN_NONE      = 0,
    OTA_SIGN_CRC32     = 1,
    OTA_SIGN_SHA256    = 2,
    OTA_SIGN_ECSDA256  = 3,
    OTA_SIGN_RSA2048   = 4,
};

/// ota version
typedef struct {
    /// vendor name
    uint32_t vendor_id;
    /// device name
    uint32_t device_id;
    /// flash id
    uint8_t flash_id;
    /// zone id
    uint8_t zone_id;
    /// ROM version
    uint16_t rom_ver;
    /// ota version
    uint32_t version;
    /// build date
    uint32_t date;
} ls_ota_ver_t;


/// ota partition header
typedef struct {
    /// partition valid flag, default to 0xffffff, will be written by ota program after data check
    uint32_t valid_flag;
    /// ota version
    ls_ota_ver_t version;
    /// flags, bit0 -- OTA mode
    uint16_t flags;
    /// partition address
    uint32_t address;
    /// partition size
    uint32_t size;
    /// program entry
    uint32_t entry;
    /// reserved
    uint32_t reserved[4];
    /// checksum of partition
    uint32_t checksum;
    /// crc32 of the partition
    uint32_t crc32;
    /// signature data
    uint32_t sign[0];
} ls_ota_header_t ;


/// OTA_OTA_START command payload
typedef struct {
    /// flags, bit0 -- OTA mode
    uint16_t flags;
    /// partition address
    uint32_t address;
    /// partition size
    uint32_t size;
    /// crypto handler
    void *crypto_handler;
} ls_ota_start_cmd_t;

/// OTA_WRITE_DATA command payload
typedef struct {
    /// package index
    uint16_t index;
    /// offset to the start of image
    uint32_t address;
    /// crc32 of the payload
    uint32_t crc32;
    /// size of the data
    uint16_t length;
    /// data buff
    uint8_t  *data;
} ls_ota_data_t;

/// OTA_OTA_VERIFY command payload
typedef struct {
    /// flags, bit0 -- OTA mode
    uint16_t flags;
    /// partition address
    uint32_t address;
    /// partition size
    uint32_t size;
    /// checksum
    uint32_t checksum;
} ls_ota_verify_cmd_t;


/// ota command header
typedef struct {
    /// ota command opcode
    uint8_t opcode;
    /// length of data
    uint16_t length;
    /// command param
    union {
        /// OTA_NEW_VERSION command
        ls_ota_ver_t version;
        /// OTA_OTA_START command
        ls_ota_start_cmd_t start;
        /// OTA_WRITE_DATA command
        ls_ota_data_t data;
        /// OTA_OTA_VERIFY command
        ls_ota_verify_cmd_t verify;
    };
} ls_ota_cmd_t;


/// ota zone define
typedef struct {
    /// zone id
    uint8_t id;
    /// encryption flag
    uint8_t enc;
    /// sign mode: 0 - None, 1 - CRC32, 2 - SHA256, 3 - ECSDA256, 4 - RSA2048
    uint8_t sign_mode;
    /// zone start address
    uint32_t address;
    /// zone size
    uint32_t size;
} ls_ota_zone_t;

/// ota configure
typedef struct {
    /// vendor id
    uint32_t vendor_id;
    /// device id
    uint32_t device_id;
    /// flash base address
    uint32_t flash_base;
    /// OTA mode: 0 - Overwrite mode, 1 - switch mode
    uint8_t ota_mode;
    /// zone count
    uint8_t zone_count;
    /// zone table
    ls_ota_zone_t *zones;

} ls_ota_config_t;

typedef struct FLASH_DEV FLASH_DEV;

/// initialize ota module
uint8_t ota_initialize(FLASH_DEV *flash_dev);

uint8_t ota_uninitialize();

/// get the image version with given zone_id
const ls_ota_ver_t *ota_get_current_version(uint8_t zone_id);

/// check ota zone validation, crc32 check
uint8_t ota_check_zone_crc(ls_ota_header_t *hdr);

/// find flash zone by zone id
ls_ota_header_t *ota_find_zone(uint8_t id);

/// find in zone table
uint8_t ota_find_zone_table(uint8_t zone_id);

/// process received ota package
uint8_t ota_process_command(ls_ota_cmd_t *cmd);

#endif /* INCLUDE_OTA_OTA_H_ */
