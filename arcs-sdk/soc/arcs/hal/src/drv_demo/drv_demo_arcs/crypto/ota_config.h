//
// OTA flash partition table
//

// vendor id   "LSWF"
#define OTA_VENDOR_ID   0x4657534C
// device id   "DEMO"
#define OTA_DEVICE_ID   0x4F4D4544

// OTA mode: 0 - Overwrite mode, 1 - switch mode
#define OTA_MODE     0

// HASH and SIGN data size, not support: 0, 
#define OTA_SIGN_SIZE       128

// flash size, 4MB flash
#define OTA_FLASH_SIZE      0x00400000

// flash base address
#define OTA_FLASH_BASE_ADDR     0x08000000

// partition 0, flash boot, 32KB
#define OTA_ZONE_BOOT_ADDRESS   OTA_FLASH_BASE_ADDR
#define OTA_ZONE_BOOT_SIZE      0xc000

// partition 1, execute zone 1, ap image, 1.4MB   #0x160000
#define OTA_ZONE_EXE1_ADDRESS   (OTA_ZONE_BOOT_ADDRESS + OTA_ZONE_BOOT_SIZE)
#define OTA_ZONE_EXE1_SIZE      0x10000

// partition 2, execute zone 2, wifi image, 512KB
#define OTA_ZONE_EXE2_ADDRESS   (OTA_ZONE_EXE1_ADDRESS + OTA_ZONE_EXE1_SIZE)
#define OTA_ZONE_EXE2_SIZE      0x80000

// partition 3, execute zone 3, bt image, 704KB
#define OTA_ZONE_EXE3_ADDRESS   (OTA_ZONE_EXE2_ADDRESS + OTA_ZONE_EXE2_SIZE)
#define OTA_ZONE_EXE3_SIZE      0xB0000

// partition OTA data, 1.4MB
#define OTA_ZONE_OTA_ADDRESS   (OTA_ZONE_EXE3_ADDRESS + OTA_ZONE_EXE3_SIZE)
#define OTA_ZONE_OTA_SIZE      0x160000


// partition factory
#define OTA_ZONE_FACT_SIZE      0x1000
#define OTA_ZONE_FACT_ADDRESS   (OTA_FLASH_BASE_ADDR + OTA_FLASH_SIZE - OTA_ZONE_FACT_SIZE)

// partition user data
#define OTA_ZONE_USER_SIZE      0x4000
#define OTA_ZONE_USER_ADDRESS   (OTA_ZONE_FACT_ADDRESS - OTA_ZONE_USER_SIZE)


