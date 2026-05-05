#ifndef __UBOOT_WDT_API_H__
#define __UBOOT_WDT_API_H__

#ifdef __cplusplus
extern "C" {
#endif

int uboot_wdt_feed(void);
int uboot_wdt_enable(void);
int uboot_wdt_disable(void);

#ifdef __cplusplus
}
#endif

#endif /* __UBOOT_WDT_API_H__ */
