#include <string.h>
#include "spiflash.h"
#include "arcs_ap.h"
#include "cache.h"
#include "PSRAMManager.h"
#include "log_print.h"
#include "ipc_utils.h"
#include "spiflash.h"

#ifdef CFG_FLASH_IF
extern int32_t psram_start, psram_end;

static FLASH_DEV flash_dev;
static uint32_t flash_sec_page_offset = 0xFFFFFFFF;

// ID_MANUFACTURER, ID_DEVICE, and OFFSET
static const uint32_t flash_id_map[][3] = {
    {0x85, 0x17, 0x001000}, // PY25Q128HA
    {0x5E, 0x17, 0x001000}, // ZB25VQ128D
    {0x1C, 0x17, 0xFFD000}, // EN25QX128A
    {0x00, 0x00, 0x000000}  // End of table
};

static void flash_if_config_security_page_offset();


int32_t flash_if_init(FLASH_DEV *dev, unsigned char ud0, unsigned char ud1)
{
    int32_t ret;

    flash_dev = *dev;
#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_halt_peer_core();
#endif
    ret = flash_init(&flash_dev, ud0, ud1);
#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_resume_peer_core();
#endif

    return ret;
}

int32_t flash_if_read(size_t offset, void *data, size_t len)
{
    int32_t ret = 0;

#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_halt_peer_core();
#endif
    ret = flash_read(&flash_dev, offset, data, len);
#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_resume_peer_core();
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    if ( !ret && ((int32_t)data >= PSRAM_BASE_ADDRESS)
         && ((void*)data < (void*)&psram_start || (void*)data >= (void*)&psram_end))
    {
        vPortEnterCritical();
        HAL_FlushDCache_by_Addr((uint32_t*)data, len);
        vPortExitCritical();
    }
#endif
#endif

    return ret;
}

int32_t flash_if_write(size_t offset, const void *data, size_t len)
{
    int32_t ret = 0;

#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    if ( ((int32_t)data >= PSRAM_BASE_ADDRESS)
         && ((void*)data < (void*)&psram_start || (void*)data >= (void*)&psram_end))
    {
        vPortEnterCritical();
        HAL_InvalidateDCache_by_Addr((uint32_t*)data, len);
        vPortExitCritical();
    }
#endif
    ipc_halt_peer_core();
#endif
    ret = flash_write(&flash_dev, offset, data, len);
#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_resume_peer_core();
#endif

    return ret;
}

int32_t flash_if_erase(size_t offset, size_t size)
{
    int32_t ret = 0;

#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_halt_peer_core();
#endif
    ret = flash_erase(&flash_dev, offset, size);
#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_resume_peer_core();
#endif

    return ret;
}

int32_t flash_if_erase_page(off_t offset, size_t size)
{
    int32_t ret = 0;

#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_halt_peer_core();
#endif
    ret = flash_erase_page(&flash_dev, offset, size);
#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_resume_peer_core();
#endif

    return ret;
}

int32_t flash_if_write_protection_set(bool enable)
{
    return flash_write_protection_set(&flash_dev, enable);
}

int flash_if_security_read(off_t offset, void *data, size_t len)
{
    int32_t ret = 0;

#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_halt_peer_core();
#endif
    flash_if_config_security_page_offset();
    offset += flash_sec_page_offset; // Adjust offset based on security page offset
    ret = flash_security_read(&flash_dev, offset, data, len);
#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_resume_peer_core();
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    if ( !ret && ((int32_t)data >= PSRAM_BASE_ADDRESS)
         && ((data < (void*)&psram_start) || (data >= (void*)&psram_end)))
    {
        vPortEnterCritical();
        HAL_FlushDCache_by_Addr((uint32_t*)data, len);
        vPortExitCritical();
    }
#endif
#endif

    return ret;
}

int flash_if_security_write(off_t offset, const void *data, size_t len)
{
    int32_t ret = 0;

#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    if ( ((int32_t)data >= PSRAM_BASE_ADDRESS)
         && ((data < (void*)&psram_start) || (data >= (void*)&psram_end)))
    {
        vPortEnterCritical();
        HAL_InvalidateDCache_by_Addr((uint32_t*)data, len);
        vPortExitCritical();
    }
#endif
    ipc_halt_peer_core();
#endif
    flash_if_config_security_page_offset();
    offset += flash_sec_page_offset; // Adjust offset based on security page offset
    ret = flash_security_write(&flash_dev, offset, data, len);
#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_resume_peer_core();
#endif

    return ret;
}

int flash_if_security_erase(off_t offset)
{
    int32_t ret = 0;

#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_halt_peer_core();
#endif
    flash_if_config_security_page_offset();
    offset += flash_sec_page_offset; // Adjust offset based on security page offset
    ret = flash_security_erase(&flash_dev, offset);
#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_resume_peer_core();
#endif

    return ret;
}

static void flash_if_config_security_page_offset(void)
{
    if (flash_sec_page_offset == 0xFFFFFFFF)
    {
        flash_sec_page_offset = 0x00000000; // Default offset if not found

        uint32_t flash_id_manufacturer = 0, flash_id_device = 0;

        int ret = flash_id(&flash_dev, &flash_id_manufacturer, &flash_id_device);

        if (ret == 0) {
            for (int i = 0; flash_id_map[i][0] != 0; i++)
            {
                if (flash_id_map[i][0] == flash_id_manufacturer && flash_id_map[i][1] == flash_id_device)
                {
                    flash_sec_page_offset = flash_id_map[i][2];
                    CLOGI("got flash_sec_page_offset: %x, flash_id_manufacturer: %x\n", flash_sec_page_offset, flash_id_manufacturer);
                    break;
                }
            }
        }
    }
    return;
}

uint8_t flash_if_check_security_support(void)
{

#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_halt_peer_core();
#endif

    flash_if_config_security_page_offset();

#if defined(CFG_AMP_IPC_FLASH_AGENT) && (CFG_AMP_IPC_FLASH_AGENT == 1)
    ipc_resume_peer_core();
#endif

    return !!flash_sec_page_offset;
}
#include "log_print.h"
int8_t flash_if_erase_otp(void)
{
   int8_t ret = 0;
   off_t offset = 0;
   uint8_t otp_data[512];
   if (flash_dev.base_addr != CMN_FLASHC_BASE) {
       CLOGW("flash dev was not initialized \n");
		return -1;
	}

   if (!flash_if_check_security_support()) {
       CLOGW("Flash unsupport OTP region!\n");
       return -1;
   }
   flash_if_security_read(0, otp_data, sizeof(otp_data));
   printf("otp data start------------");
   for(uint8_t i = 0; i < 32; i++) {
        printf("%x", otp_data[i]);
   }
   printf("otp data end------------");
   flash_if_write_protection_set(false);
   ret = flash_if_security_erase(offset);
   flash_if_write_protection_set(true);
   if (ret) {
       CLOGW("erase flash OTP failed, ret=%d\n", ret);
       return -1;
   }
   else {
       CLOGI("erase flash OTP success\n");
    }

    return ret;
}

int8_t flash_if_set_otp_flag(uint32_t magic_code)
{
    int32_t ret = 0;
    off_t offset = 0;
    uint32_t write_val = magic_code;
    uint32_t read_val = 0;

    if (flash_dev.base_addr != CMN_FLASHC_BASE) {
        CLOGW("flash dev was not initialized \n");
        return -1;
    }

    if (!flash_if_check_security_support()) {
        CLOGE("Flash unsupport OTP region!\n");
        return -1;
    }
    flash_if_write_protection_set(false);
    ret = flash_if_security_erase(offset);
    if (ret) {
        CLOGE("erase flash OTP failed, ret=%d\n", ret);
        goto write_failed;
    }
    else {
        CLOGI("erase flash OTP success\n");
    }
    ret = flash_if_security_write(offset, (void *)&write_val, sizeof(write_val));
    if (ret) {
        CLOGE("write Flash OTP flag failed, ret=%d\n", ret);
        goto write_failed;
    } else
        CLOGI("write Flash OTP flag success\n");
    ret = flash_if_security_read(0, &read_val, sizeof(read_val));
    if (ret < 0 || read_val != magic_code)
    {
        CLOGE("read otp failed or invalid value(%x), ret %d\n", read_val, ret);
        goto write_failed;
    }
    flash_if_write_protection_set(true);
    return 0;
write_failed:
    flash_if_write_protection_set(true);
    return -1;
}


#endif
