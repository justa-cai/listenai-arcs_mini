#ifndef __SIP_CALL_VIEW_H__
#define __SIP_CALL_VIEW_H__

#include "lvgl.h"

/* 通话页 UI 态（与服务的 sip_svc_state_t 对应，但按显示语义归并）。 */
enum {
    SIP_UI_DIAL = 0,     /* 拨号页：显示对端，单击呼叫 */
    SIP_UI_OUTGOING,     /* 呼出中：正在呼叫/对端振铃 */
    SIP_UI_INCOMING,     /* 来电：单击接听 */
    SIP_UI_ACTIVE,       /* 通话中：远端视频 + 计时 */
    SIP_UI_ENDED,        /* 已结束：短暂显示后自动回主页 */
};

/* 创建通话页视图（黑色全屏底 + 远端视频 + 状态/提示文字）。 */
lv_obj_t *lisa_ui_sip_view_create(lv_obj_t *parent);

/* 注销视图（删计时 timer + 清空静态 ctx）。presenter 在 lv_obj_del 之前
 * 必须调用：ctx 是单例静态，页面关闭后指针全部悬垂，而 lv_timer 不随
 * 对象树删除 —— 残留 timer 下一秒就会对着已释放的 label 跑 set_text。 */
void lisa_ui_sip_view_destroy(void);

/* 切换 UI 态并更新文字；peer 为对端 IP（可 NULL）。UI 线程调用。 */
void lisa_ui_sip_view_set_state(int ui_state, const char *peer);

/* 更新远端视频帧（UI 线程调用；缓冲由通话服务持有并保持有效）。 */
void lisa_ui_sip_view_set_frame(const uint16_t *rgb565, int w, int h);

#endif
