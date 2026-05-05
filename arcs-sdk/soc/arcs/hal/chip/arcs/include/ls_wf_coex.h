#ifndef _LS_WF_COEX_H_
#define _LS_WF_COEX_H_

typedef enum {
    ///set wifi/bt slot pti config
    COEX_OP_WIFI_HIGH_PTI       = 0,    ///force wifi high pti,bt def pti
    COEX_OP_WIFI_FORCE_BT_DEF   = 1,    ///force wifi,bt default pti
    COEX_OP_WIFI_BT_FORCE       = 2,    ///force wifi/force bt
    COEX_OP_WIFI_BT_DEF         = 3,    ///wifi default pti, bt default pti
    COEX_OP_WIFI_HIGH_PTI_BTLOW = 4,    ///force wifi high pti(BT pti = 0),bt def pti

    ///coex-win(wifi/bt slot) force
    COEX_OP_FORCE_TIME_SLOT     = 0x40,
    COEX_OP_DISABLE_TIME_SLOT   = 0x41,
} coex_op;

int coex_wifi2bt_event_handler(uint8_t op_idx, uint8_t level, int time_offset_us, int dur_us, void *args);

int coex_bt_register(void *b2w_evt_hdl, unsigned int *wififlags);

void coex_win_set_policy_pti(coex_op op, uint32_t param);
int coex_win_slot_time_set(unsigned int coex_period_us, unsigned int wifi_time_us);


#endif
