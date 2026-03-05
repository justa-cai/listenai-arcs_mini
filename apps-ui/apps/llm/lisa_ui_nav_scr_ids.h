#ifndef __NAV_SCR_IDS_H__
#define __NAV_SCR_IDS_H__

enum {
    LISA_UI_NAV_SCR_ID_HOME = 0,
    LISA_UI_NAV_SCR_ID_INFO,              // 信息/二维码页面
    LISA_UI_NAV_SCR_ID_SETTING,
    LISA_UI_NAV_SCR_ID_SETTING_COMMON,    // 基础设置（音量、亮度）
    LISA_UI_NAV_SCR_ID_SETTING_WIFI,      // 网络设置
    LISA_UI_NAV_SCR_ID_SETTING_WAKEUP,    // 唤醒交互
    LISA_UI_NAV_SCR_ID_SETTING_CLOCK,     // 闹钟设置（旧版）
    LISA_UI_NAV_SCR_ID_ALARM_SUCCESS,     // 闹钟设置成功页面
    LISA_UI_NAV_SCR_ID_ALARM_RING,        // 闹钟响铃页面
    LISA_UI_NAV_SCR_ID_ALARM_CLOCK,       // 闹钟设置页面（MVP架构）
    LISA_UI_NAV_SCR_ID_QUOTA_QRCODE,      // 额度不足二维码页面
#ifdef CONFIG_OTA
    LISA_UI_NAV_SCR_ID_OTA,               // OTA检查页面
#endif

};

#endif
