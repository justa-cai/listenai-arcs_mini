#ifndef __UBOOT_RECOVERY_API_H__
#define __UBOOT_RECOVERY_API_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UBOOT_RECOVERY_MODE_SOFT = 0,
    UBOOT_RECOVERY_MODE_HARD = 1,
} uboot_recovery_mode_t;

int uboot_recovery_request(uboot_recovery_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* __UBOOT_RECOVERY_API_H__ */
