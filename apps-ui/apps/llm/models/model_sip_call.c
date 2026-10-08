#define LOG_TAG "model_sip_call"

#include "model_sip_call.h"

#include <string.h>

#include "lisa_ui.h"
#include "lisa_ui_invoke.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"

#include "sip_call_view.h"

/*
 * SIP 通话模型层：固件侧 sip_service 的状态/帧事件 -> UI 线程桥。
 * 服务回调跑在 sip_sig 任务上，这里统一投递 worq.ui 再动视图/导航
 * （LVGL 全部调用都必须在 UI 线程 —— 与 video_player 推帧同一铁律）。
 */

#ifdef LISA_UI_PLATFORM_ARCS

#include "sip_service.h"

static void sip_ui_apply_state(int svc_state)
{
    int ui_state;
    switch (svc_state) {
    case SIP_SVC_INVITING:
    case SIP_SVC_RINGING_OUT:
        ui_state = SIP_UI_OUTGOING;
        break;
    case SIP_SVC_RINGING_IN:
        ui_state = SIP_UI_INCOMING;
        break;
    case SIP_SVC_ACTIVE:
        ui_state = SIP_UI_ACTIVE;
        break;
    case SIP_SVC_ENDED:
        ui_state = SIP_UI_ENDED;
        break;
    default:
        ui_state = SIP_UI_DIAL;
        break;
    }

    /* 来电/呼出自动进入通话页；结束回主页由 ENDED 停留后的 IDLE 处理。 */
    if (ui_state == SIP_UI_INCOMING || ui_state == SIP_UI_OUTGOING) {
        if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_SIP_CALL) {
            lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_SIP_CALL);
        }
    } else if (ui_state == SIP_UI_DIAL) {
        if (lisa_ui_nav_scr_get_top_id() == LISA_UI_NAV_SCR_ID_SIP_CALL) {
            lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
        }
        return;
    }

    lisa_ui_sip_view_set_state(ui_state, sip_service_peer_ip());
}

static void sip_svc_state_bridge(int svc_state, void *user)
{
    (void)user;
    LISA_UI_INVOKE_UI_ARG_BASE(svc_state, {
        sip_ui_apply_state(_invoke_svc_state);
    });
}

/* 远端视频帧：双缓冲指针由服务持有，浅拷指针投 UI 线程即可。
 * 宏的 token 拼接不接受 &expr，先落局部指针变量。 */
struct sip_frame_msg {
    const uint16_t *frame;
    int w;
    int h;
};

static void sip_svc_frame_bridge(const uint16_t *frame, int w, int h, void *user)
{
    (void)user;
    struct sip_frame_msg msg = {.frame = frame, .w = w, .h = h};
    struct sip_frame_msg *msg_ptr = &msg;
    LISA_UI_INVOKE_UI_ARG_PTR(msg_ptr, sizeof(msg), {
        lisa_ui_sip_view_set_frame(_invoke_msg_ptr->frame,
                                   _invoke_msg_ptr->w, _invoke_msg_ptr->h);
    });
}

int model_sip_call_init(void)
{
    sip_service_set_event_cb(sip_svc_state_bridge, NULL);
    sip_service_set_frame_cb(sip_svc_frame_bridge, NULL);
    return 0;
}

int model_sip_call_dial(void)
{
    char ip[24];
    if (sip_service_peer_cfg_get(ip, sizeof(ip)) != 0 || !ip[0]) {
        return -1;
    }
    return sip_service_start_call(ip, 0);
}

int model_sip_call_answer(void)
{
    return sip_service_answer();
}

int model_sip_call_hangup(void)
{
    return sip_service_hangup();
}

const char *model_sip_call_peer(void)
{
    return sip_service_peer_ip();
}

#else /* 模拟器：无固件服务，空实现便于 UI 联调 */

int model_sip_call_init(void)
{
    return 0;
}

int model_sip_call_dial(void)
{
    return -1;
}

int model_sip_call_answer(void)
{
    return 0;
}

int model_sip_call_hangup(void)
{
    return 0;
}

const char *model_sip_call_peer(void)
{
    return "";
}

#endif
