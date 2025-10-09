#ifndef __BOOT_LOG_H__
#define __BOOT_LOG_H__

int printk(const char *format, ...);
int bootlog_init(int dbg, uint32_t baudrate);

#define BOOTLOG_LVL_NON 0
#define BOOTLOG_LVL_ERR 1
#define BOOTLOG_LVL_WRN 2
#define BOOTLOG_LVL_INF 3
#define BOOTLOG_LVL_DBG 4

#if CONFIG_BOOT_LOG_LVL_NON
#define BOOTLOG_LVL BOOTLOG_LVL_NON
#elif CONFIG_BOOT_LOG_LVL_ERR
#define BOOTLOG_LVL BOOTLOG_LVL_ERR
#elif CONFIG_BOOT_LOG_LVL_WRN
#define BOOTLOG_LVL BOOTLOG_LVL_WRN
#elif CONFIG_BOOT_LOG_LVL_INF
#define BOOTLOG_LVL BOOTLOG_LVL_INF
#elif CONFIG_BOOT_LOG_LVL_DBG
#define BOOTLOG_LVL BOOTLOG_LVL_DBG
#endif

#ifndef BOOTLOG_LVL
#define BOOTLOG_LVL BOOTLOG_LVL_ERR
#endif

#if BOOTLOG_LVL >= BOOTLOG_LVL_ERR
#define bootlog_err(fmt, args...)                                                                                      \
    do {                                                                                                               \
        printk("[boot] err: " fmt, ##args);                                                                            \
    } while (0)
#else
#define bootlog_err(fmt, args...)
#endif

#if BOOTLOG_LVL >= BOOTLOG_LVL_WRN
#define bootlog_wrn(fmt, args...)                                                                                      \
    do {                                                                                                               \
        printk("[boot] wrn: " fmt, ##args);                                                                            \
    } while (0)
#else
#define bootlog_wrn(fmt, args...)
#endif

#if BOOTLOG_LVL >= BOOTLOG_LVL_INF
#define bootlog_inf(fmt, args...)                                                                                      \
    do {                                                                                                               \
        printk("[boot] inf: " fmt, ##args);                                                                            \
    } while (0)
#else
#define bootlog_inf(fmt, args...)
#endif


#if BOOTLOG_LVL >= BOOTLOG_LVL_DBG
#define bootlog_dbg(fmt, args...)                                                                                      \
    do {                                                                                                               \
        printk("[boot] dbg: " fmt, ##args);                                                                            \
    } while (0)
#else
#define bootlog_dbg(fmt, args...)
#endif

#endif
