#
# OTA flash partition table
#

# vendor id = "LSWF"
OTA_VENDOR_ID = 0x4657534C
# device id = "DEMO"
OTA_DEVICE_ID = 0x4F4D4544

# OTA mode: 0 - Overwrite mode, 1 - switch mode
OTA_MODE   = 0

# HASH and SIGN data size, not support: 0, 
OTA_SIGN_SIZE     = 128

# flash size, 4MB flash
OTA_FLASH_SIZE    = 0x00400000

# flash base address
OTA_FLASH_BASE_ADDR   = 0x08000000

# partition 0, flash boot, 32KB
OTA_ZONE_BOOT_ADDRESS = OTA_FLASH_BASE_ADDR
OTA_ZONE_BOOT_SIZE    = 0xc000

# partition 1, execute zone 1, ap image, 1.4MB   #0x160000
OTA_ZONE_EXE1_ADDRESS = (OTA_ZONE_BOOT_ADDRESS + OTA_ZONE_BOOT_SIZE)
OTA_ZONE_EXE1_SIZE    = 0x10000

# partition 2, execute zone 2, wifi image, 512KB
OTA_ZONE_EXE2_ADDRESS = (OTA_ZONE_EXE1_ADDRESS + OTA_ZONE_EXE1_SIZE)
OTA_ZONE_EXE2_SIZE    = 0x80000

# partition 3, execute zone 3, bt image, 704KB
OTA_ZONE_EXE3_ADDRESS = (OTA_ZONE_EXE2_ADDRESS + OTA_ZONE_EXE2_SIZE)
OTA_ZONE_EXE3_SIZE    = 0xB0000

# partition OTA data, 1.4MB
OTA_ZONE_OTA_ADDRESS = (OTA_ZONE_EXE3_ADDRESS + OTA_ZONE_EXE3_SIZE)
OTA_ZONE_OTA_SIZE    = 0x160000


# partition factory
OTA_ZONE_FACT_SIZE    = 0x1000
OTA_ZONE_FACT_ADDRESS = (OTA_FLASH_BASE_ADDR + OTA_FLASH_SIZE - OTA_ZONE_FACT_SIZE)

# partition user data
OTA_ZONE_USER_SIZE    = 0x4000
OTA_ZONE_USER_ADDRESS = (OTA_ZONE_FACT_ADDRESS - OTA_ZONE_USER_SIZE)


