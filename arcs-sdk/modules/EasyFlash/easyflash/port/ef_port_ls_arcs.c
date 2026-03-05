#include <stdarg.h>
#include <assert.h>
#include <string.h>

#include <easyflash.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "lisa_flash.h"

#include "lisa_log.h"
#include "sysheap.h"

#include "ef_def.h"

static const ef_env default_env_set[] = {
    {"ef_test", "1"},
};

#define FLASH_DEVICE_NAME "flash0"
lisa_device_t *flash_dev = NULL;

static SemaphoreHandle_t flash_lock = NULL;

EfErrCode ef_port_init(ef_env const **default_env, size_t *default_env_size)
{
    int r;

    *default_env = default_env_set;
    *default_env_size = sizeof(default_env_set) / sizeof(default_env_set[0]);

    flash_dev = lisa_device_get(FLASH_DEVICE_NAME);
    EF_ASSERT(flash_dev != NULL);

    flash_lock = xSemaphoreCreateMutex();
    EF_ASSERT(flash_lock != NULL);

    return EF_NO_ERR;
}

EfErrCode ef_port_read(uint32_t addr, uint32_t *buf, size_t size)
{
    int r = 0;
    EF_ASSERT(addr >= 0x30000000);
    EF_ASSERT(buf);

    r = lisa_flash_read(flash_dev, addr - 0x30000000, buf, size);

    return (r != 0) ? EF_READ_ERR : EF_NO_ERR;
}

EfErrCode ef_port_erase(uint32_t addr, size_t size)
{
    int r;
    EF_ASSERT(addr % EF_ERASE_MIN_SIZE == 0);
    EF_ASSERT(addr >= 0x30000000);

    r = lisa_flash_erase(flash_dev, addr - 0x30000000, size);
    if (r != 0) {
        return EF_ERASE_ERR;
    }

    return EF_NO_ERR;
}

EfErrCode ef_port_write(uint32_t addr, const uint32_t *buf, size_t size)
{
    int r;
    uint8_t *w_buf = (uint8_t *)buf;

    EF_ASSERT(addr >= 0x30000000);
    EF_ASSERT(buf);

    r = lisa_flash_write(flash_dev, addr - 0x30000000, buf, size);

    if (r != 0) {
        return EF_WRITE_ERR;
    }

    return EF_NO_ERR;
}

void ef_port_env_lock(void)
{
    assert(flash_lock != NULL);
    xSemaphoreTake(flash_lock, portMAX_DELAY);
}

void ef_port_env_unlock(void)
{
    assert(flash_lock != NULL);
    xSemaphoreGive(flash_lock);
}

void ef_log_debug(const char *file, const long line, const char *format, ...)
{

#ifdef PRINT_DEBUG

    va_list args;

    va_start(args, format);

    char *p;
    size_t len;

    len = vsnprintf(NULL, (size_t)0, format, args);
    p = (char *)psram_malloc(len + 1);
    if (!p) {
        return;
    }
    vsnprintf(p, len + 1, format, args);
    LOGI("%s", p);
    psram_free(p);

    va_end(args);

#endif
}

void ef_log_info(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    char *p;
    size_t len;

    len = vsnprintf(NULL, (size_t)0, format, args);
    p = (char *)psram_malloc(len + 1);
    if (!p) {
        return;
    }
    vsnprintf(p, len + 1, format, args);
    LOGI("%s", p);
    psram_free(p);

    va_end(args);
}

void ef_print(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    vprintk(format, args);

    va_end(args);
}
