/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file lisa_bt_classic_api_local.c
 * @brief Local implementation of unified BT Classic API (AP core / single-core).
 *        Directly sends events to the BT OS task via btos_send_event().
 */

#include "lisa_bt_classic_api.h"

#include <string.h>
#include <stddef.h>

#include "btos_def.h"
#include "btos_al.h"
#include "bt_os_task.h"

/*
 * Helper: allocate btos_event, set msg_id, copy param, send to local BT OS task.
 */
static uint8_t bt_classic_send_event(uint16_t msg_id, void *param, uint16_t param_len)
{
    btos_event_t ev;
    uint16_t ev_len = sizeof(btos_msg_t) + param_len;

    ev.msg_body = btos_malloc(ev_len);
    if (ev.msg_body == NULL) {
        return 0xff;
    }
    ev.msg_body->msg_id = msg_id;
    ev.msg_body->param_len = param_len;
    if (param_len > 0 && param != NULL) {
        memcpy(ev.msg_body->param, param, param_len);
    }

    return (btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY) == pdTRUE) ? 0 : 1;
}

/*
 * Helper: allocate btos_event with variable-length payload.
 * Caller fills param area via *out_param, then calls bt_classic_commit_varlen_event().
 */
static btos_event_t bt_classic_alloc_varlen_event(uint16_t msg_id, uint16_t param_len,
                                                  void **out_param)
{
    btos_event_t ev;
    uint16_t ev_len = sizeof(btos_msg_t) + param_len;

    ev.msg_body = btos_malloc(ev_len);
    if (ev.msg_body == NULL) {
        *out_param = NULL;
        return ev;
    }
    ev.msg_body->msg_id = msg_id;
    ev.msg_body->param_len = param_len;

    *out_param = ev.msg_body->param;
    return ev;
}

static uint8_t bt_classic_commit_varlen_event(btos_event_t *ev)
{
    return (btos_send_event(OS_TASK_ID_BT, ev, (uint32_t)BTOS_TASK_MAX_DELAY) == pdTRUE) ? 0 : 1;
}

/*
 * A2DP API
 ****************************************************************************************
 */

uint8_t lisa_bt_a2dp_enable(const void *cfg)
{
    const bt_a2dp_enable_info_t *p = (const bt_a2dp_enable_info_t *)cfg;
    bt_a2dp_enable_info_t info = {
        .a2dp_role = p->a2dp_role,
        .aac_support = p->aac_support,
    };
    return bt_classic_send_event(BT_OS_A2DP_ENABLE_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_a2dp_connect(uint8_t conidx, uint8_t role)
{
    bt_a2dp_connect_info_t info = {
        .conidx = conidx,
        .role = role,
    };
    return bt_classic_send_event(BT_OS_A2DP_CONNECT_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_a2dp_start(uint8_t conidx)
{
    bt_a2dp_start_info_t info = {
        .conidx = conidx,
    };
    return bt_classic_send_event(BT_OS_A2DP_START_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_a2dp_send_media(uint8_t conidx, uint8_t frame_num, uint16_t len, const uint8_t *data)
{
    uint16_t param_len = sizeof(bt_a2dp_send_media_info_t) + len;
    void *param;
    btos_event_t ev = bt_classic_alloc_varlen_event(BT_OS_A2DP_SEND_MEDIA_EVT, param_len, &param);

    if (param == NULL) {
        return 0xff;
    }

    bt_a2dp_send_media_info_t *info = (bt_a2dp_send_media_info_t *)param;
    info->conidx = conidx;
    info->frame_num = frame_num;
    info->len = len;
    memcpy(info->data, data, len);

    return bt_classic_commit_varlen_event(&ev);
}

/*
 * HFP API
 ****************************************************************************************
 */

uint8_t lisa_bt_hfp_enable(const void *cfg)
{
    const bt_hfp_enable_info_t *p = (const bt_hfp_enable_info_t *)cfg;
    bt_hfp_enable_info_t info = {
        .hfp_role = p->hfp_role,
        .hfp_feats = p->hfp_feats,
    };
    return bt_classic_send_event(BT_OS_HFP_ENABLE_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_hfp_connect(uint8_t conidx, uint8_t peer_role)
{
    bt_hfp_connect_info_t info = {
        .conidx = conidx,
        .peer_role = peer_role,
    };
    return bt_classic_send_event(BT_OS_HFP_CONNECT_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_hfp_set_codec_type(uint8_t conidx, uint8_t codec_type)
{
    bt_hfp_set_codec_info_t info = {
        .conidx = conidx,
        .codec_type = codec_type,
    };
    return bt_classic_send_event(BT_OS_HFP_SET_CODEC_TYPE_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_hfp_call_start(uint8_t conidx, uint8_t call_idx)
{
    bt_hfp_call_start_info_t info = {
        .conidx = conidx,
        .call_idx = call_idx,
    };
    return bt_classic_send_event(BT_OS_HFP_CALL_START_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_hfp_call_add_audio(uint8_t conidx, uint8_t codec_type)
{
    bt_hfp_call_add_audio_info_t info = {
        .conidx = conidx,
        .codec_type = codec_type,
    };
    return bt_classic_send_event(BT_OS_HFP_CALL_ADD_AUDIO_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_hfp_call_remove_audio(uint8_t conidx, uint8_t reason)
{
    bt_hfp_call_remove_audio_info_t info = {
        .conidx = conidx,
        .reason = reason,
    };
    return bt_classic_send_event(BT_OS_HFP_CALL_REMOVE_AUDIO_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_hfp_call_incoming(uint8_t conidx, uint8_t idx)
{
    bt_hfp_call_incomming_info_t info = {
        .conidx = conidx,
        .idx = idx,
    };
    return bt_classic_send_event(BT_OS_HFP_CALL_INCOMMING_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_hfp_send_audio(uint8_t conidx, uint16_t len, const uint8_t *data)
{
    uint16_t param_len = sizeof(bt_hfp_send_aud_info_t) + len;
    void *param;
    btos_event_t ev = bt_classic_alloc_varlen_event(BT_OS_HFP_SEND_AUD_EVT, param_len, &param);

    if (param == NULL) {
        return 0xff;
    }

    bt_hfp_send_aud_info_t *info = (bt_hfp_send_aud_info_t *)param;
    info->conidx = conidx;
    info->len = len;
    memcpy(info->data, data, len);

    return bt_classic_commit_varlen_event(&ev);
}

/*
 * AVRCP API
 ****************************************************************************************
 */

uint8_t lisa_bt_avrcp_play_status_set(uint8_t conidx, uint8_t play_status)
{
    bt_avrcp_play_status_info_t info = {
        .conidx = conidx,
        .play_status = play_status,
    };
    return bt_classic_send_event(BT_OS_AVRCP_PLAY_STATUS_SET_EVT, &info, sizeof(info));
}

/*
 * GAP API
 ****************************************************************************************
 */

uint8_t lisa_bt_gap_auth_req(uint8_t conidx, uint8_t sec_req)
{
    bt_gap_auth_req_info_t info = {
        .conidx = conidx,
        .sec_req = sec_req,
    };
    return bt_classic_send_event(BT_OS_GAP_AUTH_REQ_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_gap_save_link_key(uint8_t conidx)
{
    bt_gap_save_lk_info_t info = {
        .conidx = conidx,
    };
    return bt_classic_send_event(BT_OS_GAP_SAVE_LK_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_set_asic_cvsd(uint8_t enable)
{
    bt_set_asic_cvsd_info_t info = {
        .enable = enable,
    };
    return bt_classic_send_event(BT_OS_BT_SET_ASIC_CVSD_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_connect(const gap_bdaddr_t *addr, uint8_t type, uint16_t clk_off, uint8_t page_scan_rep_mode)
{
    bt_connect_info_t info = {
        .addr = *addr,
        .type = type,
        .clk_off = clk_off,
        .page_scan_rep_mode = page_scan_rep_mode,
    };
    return bt_classic_send_event(BT_OS_BT_CONNECT_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_inquiry_start(uint8_t disc_mode, uint8_t max_count)
{
    bt_inquiry_info_t info = {
        .disc_mode = disc_mode,
        .max_count = max_count,
    };
    return bt_classic_send_event(BT_OS_BT_INQ_START_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_inquiry_stop(void)
{
    return bt_classic_send_event(BT_OS_BT_INQ_STOP_EVT, NULL, 0);
}

uint8_t lisa_bt_scan(uint8_t enable)
{
    bt_scan_info_t info = {
        .scan_en = enable,
    };
    return bt_classic_send_event(BT_OS_BT_SCAN_EVT, &info, sizeof(info));
}

/*
 * Audio HAL API
 ****************************************************************************************
 */

uint8_t lisa_bt_a2dp_audio_start(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate)
{
    bt_aud_a2dp_send_start_info_t info = {
        .conidx = conidx,
        .codec = codec,
        .ch = ch,
        .sample_rate = sample_rate,
    };
    return bt_classic_send_event(BT_OS_AUD_A2DP_SEND_START_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_a2dp_audio_stop(uint8_t conidx, uint8_t status)
{
    bt_aud_a2dp_send_stop_info_t info = {
        .conidx = conidx,
        .status = status,
    };
    return bt_classic_send_event(BT_OS_AUD_A2DP_SEND_STOP_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_a2dp_audio_send(uint8_t conidx, uint8_t frame_num, uint16_t seq, uint16_t len, const uint8_t *data)
{
    uint16_t param_len = sizeof(bt_aud_a2dp_send_data_info_t) + len;
    void *param;
    btos_event_t ev = bt_classic_alloc_varlen_event(BT_OS_AUD_A2DP_SEND_DATA_EVT, param_len, &param);

    if (param == NULL) {
        return 0xff;
    }

    bt_aud_a2dp_send_data_info_t *info = (bt_aud_a2dp_send_data_info_t *)param;
    info->conidx = conidx;
    info->frame_num = frame_num;
    info->seq = seq;
    info->len = len;
    memcpy(info->data, data, len);

    return bt_classic_commit_varlen_event(&ev);
}

uint8_t lisa_bt_hfp_audio_start(uint8_t conidx, uint8_t codec_type)
{
    bt_aud_hfp_send_start_info_t info = {
        .conidx = conidx,
        .codec_type = codec_type,
    };
    return bt_classic_send_event(BT_OS_AUD_HFP_SEND_START_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_hfp_audio_stop(uint8_t conidx, uint8_t reason)
{
    bt_aud_hfp_send_stop_info_t info = {
        .conidx = conidx,
        .reason = reason,
    };
    return bt_classic_send_event(BT_OS_AUD_HFP_SEND_STOP_EVT, &info, sizeof(info));
}

uint8_t lisa_bt_hfp_audio_send(uint8_t conidx, uint8_t pkt_sta, uint16_t len, const uint8_t *data)
{
    uint16_t param_len = sizeof(bt_aud_hfp_send_data_info_t) + len;
    void *param;
    btos_event_t ev = bt_classic_alloc_varlen_event(BT_OS_AUD_HFP_SEND_DATA_EVT, param_len, &param);

    if (param == NULL) {
        return 0xff;
    }

    bt_aud_hfp_send_data_info_t *info = (bt_aud_hfp_send_data_info_t *)param;
    info->conidx = conidx;
    info->pkt_sta = pkt_sta;
    info->len = len;
    memcpy(info->data, data, len);

    return bt_classic_commit_varlen_event(&ev);
}

/*
 * BLE whitelist / resolve list API
 ****************************************************************************************
 */

uint8_t lisa_bt_ble_add_paired_to_wlist(void)
{
    return bt_classic_send_event(BT_OS_BLE_ADD_WLIST_EVT, NULL, 0);
}

uint8_t lisa_bt_ble_add_paired_to_rlist(void)
{
    return bt_classic_send_event(BT_OS_BLE_ADD_RLIST_EVT, NULL, 0);
}
