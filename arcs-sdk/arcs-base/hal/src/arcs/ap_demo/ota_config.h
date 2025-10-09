//
// OTA flash partition table
//

// vendor id   "LSWF"
#define OTA_VENDOR_ID   0x4657534C
// device id   "DEMO"
#define OTA_DEVICE_ID   0x4F4D4544

// OTA mode: 0/OTA_MODE_OVERWRITE - Overwrite mode, 1/OTA_MODE_SWITCH - switch mode
#define OTA_MODE     OTA_MODE_SWITCH

// flash size, 4MB flash
#define OTA_FLASH_SIZE      0x00400000

// partition 0, flash boot, 64KB
#define OTA_ZONE_BOOT_SIZE      0x10000
#define OTA_ZONE_BOOT_ENC       FLASH_ENC_ALL
#define OTA_ZONE_BOOT_SIGN      0

// partition 1, execute zone 1, ap image, 1.2MB
#define OTA_ZONE_AP_SIZE        0x140000
#define OTA_ZONE_AP_ENC         0
#define OTA_ZONE_AP_SIGN        0

// partition 2, execute zone 2, cp image, 1.2MB
#define OTA_ZONE_CP_SIZE        0x140000
#define OTA_ZONE_CP_ENC         0
#define OTA_ZONE_CP_SIGN        0

// partition OTA data, 1.2MB
#define OTA_ZONE_OTA_SIZE       0x140000
#define OTA_ZONE_OTA_ENC        0         // OTA zone enc depend on the target zone (AP/CP)
#define OTA_ZONE_OTA_SIGN       0         // OTA zone sign depend on the target zone (AP/CP)

// partition user data
#define OTA_ZONE_USER_SIZE      0x6000
#define OTA_ZONE_USER_ENC       1
#define OTA_ZONE_USER_SIGN      0

// partition factory
#define OTA_ZONE_FACT_SIZE      0x2000
#define OTA_ZONE_FACT_ENC       1
#define OTA_ZONE_FACT_SIGN      0



// partition offset from flash base
#define OTA_ZONE_BOOT_ADDRESS   0
#define OTA_ZONE_AP_ADDRESS    (OTA_ZONE_BOOT_ADDRESS + OTA_ZONE_BOOT_SIZE)
#define OTA_ZONE_CP_ADDRESS    (OTA_ZONE_AP_ADDRESS + OTA_ZONE_AP_SIZE)
#define OTA_ZONE_OTA_ADDRESS   (OTA_ZONE_CP_ADDRESS + OTA_ZONE_CP_SIZE)
#define OTA_ZONE_USER_ADDRESS  (OTA_ZONE_FACT_ADDRESS - OTA_ZONE_USER_SIZE)
#define OTA_ZONE_FACT_ADDRESS  (OTA_FLASH_SIZE - OTA_ZONE_FACT_SIZE)

