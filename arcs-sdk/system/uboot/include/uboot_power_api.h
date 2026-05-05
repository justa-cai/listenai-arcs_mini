#ifndef __UBOOT_POWER_API_H__
#define __UBOOT_POWER_API_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 置位 boot_info.shutdown_req 后触发软复位，stage0 电源守护识别标志后释放
 * PWR_LOCK 并自旋等 POWER_KEY 长按重启。用于 USB 维持 VCC 无法真关机的场景。*/
void uboot_shutdown_request(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* __UBOOT_POWER_API_H__ */
