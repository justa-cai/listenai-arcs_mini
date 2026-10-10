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

#include "FreeRTOS.h"
#include "semphr.h"

#include "extra/others/snapshot/lv_snapshot.h"

#include "sip_service.h"
#include "sip_screen.h"

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

/*
 * LCD 回传抓帧（sip_screen 配套）：lv_snapshot 必须在 UI 线程执行，
 * 这里投递 worq.ui 做整屏重绘，调用方（sip_scr 任务）用信号量同步等
 * 结果。ctx 用静态单例 —— grab 只在 sip_scr 任务串行调用，且 worq
 * 拿到的是指针副本、操作的是同一块内存。
 */
struct sip_grab_ctx {
    void *dst;
    uint32_t cap;
    uint16_t w, h;
    bool ok;
    SemaphoreHandle_t done;
};

static struct sip_grab_ctx s_grab;

static void sip_ui_do_grab(void *arg, uint32_t len)
{
    (void)len;
    struct sip_grab_ctx *c = arg;
    lv_img_dsc_t dsc;
    c->ok = lv_snapshot_take_to_buf(lv_scr_act(), LV_IMG_CF_TRUE_COLOR,
                                    &dsc, c->dst, c->cap) == LV_RES_OK;
    if (c->ok) {
        c->w = dsc.header.w;
        c->h = dsc.header.h;
    }
    xSemaphoreGive(c->done);
}

static bool sip_svc_grab_screen(void *dst, uint32_t cap, uint16_t *w, uint16_t *h,
                                void *user)
{
    (void)user;
    if (!s_grab.done) {
        s_grab.done = xSemaphoreCreateBinary();
        if (!s_grab.done) {
            return false;
        }
    }
    /* 清掉上次超时残留的完成信号，防止本次拿到旧结果。 */
    xSemaphoreTake(s_grab.done, 0);
    s_grab.dst = dst;
    s_grab.cap = cap;
    s_grab.ok = false;
    s_grab.w = s_grab.h = 0;
    struct sip_grab_ctx *p = &s_grab;
    /* 用 ARG_BASE 传指针值本身：ARG_PTR 会把结构体拷进 worq 堆副本，
     * do_grab 写在副本上，ok/w/h 随副本被 free —— 调用方永远读不到
     * 结果（实测每秒 "grab failed" 循环）。指针值直达则操作同一块。
     * 代码块里必须用 _invoke_p（从 worq 参数解出、指向静态 s_grab）：
     * 闭包变量 p 在 take 超时返回后对延迟执行的 worq 是悬垂的。 */
    LISA_UI_INVOKE_UI_ARG_BASE(p, {
        sip_ui_do_grab(_invoke_p, 0);
    });
    if (xSemaphoreTake(s_grab.done, pdMS_TO_TICKS(800)) != pdTRUE) {
        return false;
    }
    if (!s_grab.ok) {
        return false;
    }
    *w = s_grab.w;
    *h = s_grab.h;
    return true;
}

int model_sip_call_init(void)
{
    sip_service_set_event_cb(sip_svc_state_bridge, NULL);
    sip_service_set_frame_cb(sip_svc_frame_bridge, NULL);
    sip_screen_set_grab_cb(sip_svc_grab_screen, NULL);
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
