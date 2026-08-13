#ifndef LISA_SYS_REBOOT_H_
#define LISA_SYS_REBOOT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_REBOOT_SOFT 0
#define SYS_REBOOT_HARD 1

void sys_reboot(int type) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif
