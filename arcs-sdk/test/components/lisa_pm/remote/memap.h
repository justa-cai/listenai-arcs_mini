#ifndef __WIFI_PM_AP_MEMAP_H__
#define __WIFI_PM_AP_MEMAP_H__

#define __KB__(x) ((x) * 1024)
#define __MB__(x) ((x) * 1024 * 1024)

/* AP 镜像位于 flash 起始 1MB */
#define MEM_AP_FLASH_BASE  0x30000000
#define MEM_AP_FLASH_SIZE  __MB__(1)

/* CP 镜像紧随 AP，从 0x30100000 起 */
#define MEM_CP_FLASH_BASE  (MEM_AP_FLASH_BASE + MEM_AP_FLASH_SIZE)

#endif /* __WIFI_PM_AP_MEMAP_H__ */
