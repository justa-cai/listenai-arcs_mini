#include <stdarg.h>
#include <assert.h>
#include <string.h>

#include <easyflash.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "flash_if.h"
#include "chip.h"

#include "lisa_log.h"
#include "sysheap.h"

#include "ef_def.h"

static const ef_env default_env_set[] = {
    {"ef_test", "1"},
};

static FLASH_DEV flash0 = {
	.base_addr = CMN_FLASHC_BASE,
	.d_width = 4,
	.sclk_div = 0xFF,
	.run_mod = RUN_WITHOUT_INT,
	.timeout = 2000000,
	.addr_bytes = 3,
	.addr_auto = 0,
};

static SemaphoreHandle_t flash_lock = NULL;

EfErrCode ef_port_init(ef_env const **default_env, size_t *default_env_size)
{
    int r;

    *default_env = default_env_set;
    *default_env_size = sizeof(default_env_set) / sizeof(default_env_set[0]);

    r = flash_if_init(&flash0, 0, 0);
    EF_ASSERT(r == 0);

    flash_lock = xSemaphoreCreateMutex();
    EF_ASSERT(flash_lock != NULL);

    return EF_NO_ERR;
}

EfErrCode ef_port_read(uint32_t addr, uint32_t *buf, size_t size)
{
    int r = 0;
    EF_ASSERT(addr >= 0x30000000);
    EF_ASSERT(buf);

    flash_if_write_protection_set(false);
    r = flash_if_read(addr - 0x30000000, buf, size);
    flash_if_write_protection_set(true);

    return (r != 0) ? EF_READ_ERR : EF_NO_ERR;
}

EfErrCode ef_port_erase(uint32_t addr, size_t size)
{
    int r;
    EF_ASSERT(addr % EF_ERASE_MIN_SIZE == 0);
    EF_ASSERT(addr >= 0x30000000);

    flash_if_write_protection_set(false);
    r = flash_if_erase(addr - 0x30000000, size);
    flash_if_write_protection_set(true);
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

    if ((uint32_t)buf >= 0x30000000) {
        w_buf = (uint8_t *)psram_malloc(size);
        if (!w_buf) {
            return EF_WRITE_ERR;
        }
        memcpy(w_buf, buf, size);
    }

    flash_if_write_protection_set(false);
    r = flash_if_write(addr - 0x30000000, w_buf, size);
    flash_if_write_protection_set(true);

    if ((uint32_t)buf >= 0x30000000) {
        psram_free(w_buf);
    }

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
