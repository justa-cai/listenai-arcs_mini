/****************************************************************************************
 *
 * @file wifi_ps_hw.h
 *
 * @brief WiFi power save
 *
 * Copyright (C) ListenAI 2024
 *
 * Created on: Dec 7, 2024
 *
 *
 ****************************************************************************************
 */
#ifndef _WIFI_PS_HW_H_
#define _WIFI_PS_HW_H_



struct wifi_ps_hw_ops {
    /*Before entering sleep, it is necessary to set the clock, power domains,
     * and other operations that depend on the hardware of the system platform*/
    int32_t (*suspend)(uint32_t sleep_time, int32_t suspend);
    /*Operations to wakeup wifi hardware*/
    int32_t (*resume)(int32_t suspend);
    int32_t (*check_idle)(void);
    void (*aon_wakup_isr)(void);
    void (*mac_wakup_isr)(void);
    void (*set_beacon_intv)(uint16_t bcn_intv);
    void (*set_listen_intv)(uint16_t listen_intv);
    void (*set_dtim)(uint8_t dtim_period);
    void (*enable_dtim_wakeup)(bool enable);
    void (*set_wakeup_time)(uint32_t time);
};


int32_t wifi_ps_hw_init(void);

#endif
