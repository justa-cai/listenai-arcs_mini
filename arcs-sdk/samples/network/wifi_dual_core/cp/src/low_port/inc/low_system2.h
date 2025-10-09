
#ifndef __low_system2_h__
#define __low_system2_h__

#include "stdint.h"

#define LOG_MAX_FILE 10
#define LOG_FILE_NAME_LEN 36
#if __cplusplus
extern "C"
{
#endif
    /*
     * 通知tee保存log
     */
    void low_system_flush_log(void);

    int low_system_sn_set(char *sn);
    char *low_system_sn_get(void);

#define LOW_SYSTEM_BOOTLOGO_LEFT 0
#define LOW_SYSTEM_BOOTLOGO_RIGHT 1
    /*
     * 设置左右手模式后，开机logo方向修改
     */
    int32_t low_system_set_bootlogo(int32_t lr);

    /*
     *设置USB相关
     */
    int32_t low_system_usb_init(void);
    int32_t low_system_set_usb_mode(int32_t mode);
    int32_t low_system_get_usb_state(void);
    int32_t low_system_usb_mode_get(void);
    int32_t low_system_usb_adb_ctrl(int32_t onoff);
    int32_t low_system_usb_mass_ctrl(int32_t onoff);
    int32_t low_system_usb_init_wait(void);
    int32_t low_system_usb_init_get(void);

    int32_t low_system_log_get_output_fd(void);
    int32_t low_system_log_init(int32_t is_install);

    char *low_system_pid_get(void);
    int32_t low_system_pid_save(char *id);

    int32_t low_system_bt_mac_load(void);
    void low_system_get_mac_str(char *mac_colon);
    int32_t low_system_fast_scan_ctrl(int32_t onoff);

    /*
     * U盘分区挂载,复读机初始化/逆初始化
     * 从未调用过U盘open的状态下，复制一份内置文件，否则仅仅创建空的复读机文件夹
     */
    int32_t low_system_usb_repeater_init(void);
    int32_t low_system_usb_repeater_term(void);
    int32_t low_system_usb_sinology_init(void);
    /*
     * 从未使用过U盘,repeater_mode == 0
     * 打开过U盘，repeater_mode != 0
     */
    int32_t low_system_usb_repeater_mode_reset(void);
    int32_t low_system_usb_repeater_mode_set(int32_t mode);
    int32_t low_system_usb_repeater_mode_get(void);

/*
 * 控制nmi_irq 
 * 1 开启
 * 0 关闭
 */
int32_t low_system_fast_scan_ctrl(int32_t onoff);
/*
 * 控制uboot开启关闭
 * 1 开启
 * 0 关闭
 */
int32_t low_system_uboot_enable(int32_t is_enable);
/*
 * 控制串口打印
 * 1 开启
 * 0 关闭
 */
int32_t low_system_uart_debug_ctrl(int32_t onoff);

/*
 * 防抄板校验
 * ret: SSK_SUCCESS success other fail
 *	    SSK_NO_SSK          未烧写ssk
 *	    SSK_NO_ACB_DATA     未烧写acb_data
 *	    SSK_ACB_FAIL        acb校验失败
 *	    SSK_CATCH_THIEF     分区内有acb却没有烧ssk
 */
#define SSK_SUCCESS       0
#define SSK_NO_SSK        1
#define SSK_NO_ACB_DATA   2
#define SSK_ACB_FAIL    (-1)
#define SSK_CATCH_THIEF (-2)
int32_t low_system_ssk_chk(void);

#if __cplusplus
}
#endif
#endif
