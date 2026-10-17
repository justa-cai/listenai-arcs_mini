/* llmterm —— LVGL 终端桌面（macOS Terminal 风格）+ LLM 对话 + 网络键盘输入
 *
 * 布局（240x240，仿 macOS Terminal 深色主题）：
 *   ┌──────────────────────────┐
 *   │ ● ● ●       LLM@ARCS-MINI │  标题栏（红黄绿信号灯 + 标题，h=24）
 *   │ > what is one plus one    │  问："> " 前缀回显
 *   │ One plus one is two.      │  答：无前缀，流式追加
 *   ├──────────────────────────┤
 *   │ > hello_                  │  输入行（实时逐键；↑↓ 滚动回看历史）
 *   └──────────────────────────┘
 *
 * 问答呈现：问题以 "> " 前缀回显（绿色），答案无前缀直接追加（浅色），
 * 系统提示灰显；问答对之间空行分组，像素换行交给 label（recolor 行标签）。
 *
 * 渲染节流：追加只写环形缓冲并置 dirty，由 key_timer 50ms 统一冲刷
 * （流式 piece 高频到达时合并，不逐 token 重建 label）；未闭合的
 * 当前行也参与渲染，流式半行立即可见（打字机效果完整）。
 * 输入侧卫生：丢弃 \r/C0/DEL，剥离 ANSI CSI 转义序列。
 *
 * 数据流：net_kb 按键/整行 → lv_timer 轮询 → 输入行；
 * 回车 → llm_client_ask()；流式回包（rpc_client 线程）→
 * LISA_UI_INVOKE_UI 投递 → worq.ui 追加输出（打字机）。
 */
#define TAG "llmterm"

#include <string.h>
#include <stdio.h>

#include "lvgl.h"

#include "lisa_log.h"
#include "lisa_ui_invoke.h"

#include "llm_client.h"
#include "net_kb.h"
#if CONFIG_LLMTERM_TTS
#include "tts_speak.h"
#endif

/* macOS Terminal 深色主题配色 */
#define COL_BG_TITLE   0x323233   /* 标题栏（比正文稍亮） */
#define COL_BG_SCREEN  0x1E1E1E   /* 正文背景 */
#define COL_TEXT       0xE8E8E8   /* 正文文字（纯文本统一色） */
#define COL_TITLE      0xB8B8B8   /* 标题文字 */
#define COL_Q_HEX      "27C93F"   /* 问题回显 recolor 标签色（=信号灯绿） */
#define COL_SYS_HEX    "8A8A8A"   /* 系统提示 recolor 标签色（灰） */
#define COL_LIGHT_R    0xFF5F56   /* 信号灯红 */
#define COL_LIGHT_Y    0xFFBD2E   /* 信号灯黄 */
#define COL_LIGHT_G    0x27C93F   /* 信号灯绿 */

#define TERM_TITLE       "LLM@ARCS-MINI"
#define TERM_TITLE_H     24
#define TERM_INPUT_H     20

#define TERM_LINES_KEEP  11          /* 渲染保留的逻辑行数（≈196px/17px） */
#define TERM_LINES_RING  64          /* 环形缓冲行数（回看历史容量） */
#define TERM_LINE_MAX    64          /* 单行容量；像素换行由 label WRAP 负责，
                                       * 仅缓冲满时按词断 */
#define TERM_INPUT_MAX   96          /* 云端 ASR 识别文本较长；llm mailbox 上限 480B */

static lv_obj_t *lbl_out, *lbl_in, *lbl_status;
static lv_obj_t *cont_out;           /* 固定高度裁剪容器（自动滚屏） */

enum { LT_ANS = 0, LT_Q, LT_SYS };        /* 行颜色类型：答案/问题/系统 */

static struct {
    char ring[TERM_LINES_RING][TERM_LINE_MAX];
    uint8_t type[TERM_LINES_RING];        /* 各行颜色类型（LT_*） */
    int head;                             /* 下一写入行 */
    char cur[TERM_LINE_MAX];              /* 当前行（未闭合）部分缓冲 */
    int curlen;
    uint8_t line_type;                    /* 当前行颜色类型 */
    char inbuf[TERM_INPUT_MAX + 1];       /* 输入行缓冲 */
    int inlen;
    uint8_t busy;                         /* LLM 生成中（回车忽略） */
    uint8_t scroll;                       /* 历史回滚行数（0=贴底） */
    uint8_t new_cnt;                      /* 回看期间新到的行数（提示用） */
    uint8_t esc_pending;                  /* ↑↓ 转义序列状态 */
    uint8_t ansi_st;                      /* 输出 ANSI CSI 剥离状态机 */
    uint8_t dirty;                        /* 输出待重绘（key_timer 冲刷） */
} T;

/* ------------------------------------------------ 行环形缓冲 */
static void ring_put(const char *s, int len, uint8_t type)
{
    if (len >= TERM_LINE_MAX) len = TERM_LINE_MAX - 1;
    char *dst = T.ring[T.head];
    memcpy(dst, s, len);
    dst[len] = '\0';
    T.type[T.head] = type;
    T.head = (T.head + 1) % TERM_LINES_RING;
}

/* 渲染：环形缓冲非空行 + 未闭合当前行 → recolor 行标签 → 单 label。
 * 追加侧只置 dirty，由 key_timer 50ms 统一冲刷（流式合并渲染）；
 * ↑↓ 滚动键会直接调用（交互即时反馈）。 */
static void ring_render(void)
{
    static char render[TERM_LINES_KEEP * (TERM_LINE_MAX + 12) + 32];

    static int list[TERM_LINES_RING + 1];  /* static：timer 任务栈省 260B */
    int used_lines = 0;
    int oldest = -1;
    for (int i = TERM_LINES_RING; i >= 1; i--) {
        int idx = (T.head - i + TERM_LINES_RING) % TERM_LINES_RING;
        if (T.ring[idx][0]) { oldest = i; break; }
    }
    for (int i = oldest; i >= 1; i--)
        list[used_lines++] = (T.head - i + TERM_LINES_RING) % TERM_LINES_RING;
    if (T.curlen)                                    /* 流式中的半行也上屏 */
        list[used_lines++] = -1;
    if (used_lines == 0) {
        lv_label_set_text(lbl_out, "");
        return;
    }

    /* 滚动窗口：末尾往前 scroll 行，再取 KEEP 行 */
    int end = used_lines - T.scroll;
    if (end < 1) end = 1;
    int begin = end - TERM_LINES_KEEP;
    if (begin < 0) begin = 0;

    size_t used = 0;
    render[0] = '\0';
    for (int i = begin; i < end; i++) {
        int idx = list[i];
        const char *s = (idx < 0) ? T.cur : T.ring[idx];
        const char *pre = "", *suf = "";
        uint8_t t = (idx < 0) ? LT_ANS : T.type[idx];
        /* LVGL8 recolor 语法：#RRGGBB 后必须跟空格作参数终结符，
         * 否则解析器把正文首词当参数吃掉（行首字符丢失） */
        if (t == LT_Q) { pre = "#" COL_Q_HEX " "; suf = " #"; }
        else if (t == LT_SYS) { pre = "#" COL_SYS_HEX " "; suf = " #"; }
        int pl = strlen(pre), sl = strlen(suf);
        int l = strlen(s);
        if (used + pl + l + sl + 1 >= sizeof render) break;
        memcpy(render + used, pre, pl); used += pl;
        memcpy(render + used, s, l); used += l;
        memcpy(render + used, suf, sl); used += sl;
        render[used++] = '\n';
    }
    if (used) used--;                             /* 去掉末尾换行 */
    render[used] = '\0';
    lv_label_set_text(lbl_out, render);
    /* 文本增长（流式半行）时重跑一次对齐：LVGL8 对齐偏移缓存与
     * recolor 解析在动态文本下会留下重叠残影（表现为输出期间上方
     * 数行乱码，DONE 全量重绘后才消失），强制重对齐触发完整失效 */
    lv_obj_align(lbl_out, LV_ALIGN_BOTTOM_LEFT, 3, 0);
}

static void term_append(const char *s, int len)
{
    int closed = 0;                       /* 本次闭合的行数（滚动锚定用） */
    if (len <= 0) return;
    for (int i = 0; i < len; i++) {
        char c = s[i];

        /* ANSI CSI 序列剥离：ESC '[' .. 终止字节(0x40..0x7E) */
        if (T.ansi_st == 1) {
            T.ansi_st = (c == '[') ? 2 : 0;
            continue;
        }
        if (T.ansi_st == 2) {
            if (c >= 0x40 && c <= 0x7e) T.ansi_st = 0;
            continue;
        }
        if (c == 0x1b) { T.ansi_st = 1; continue; }
        if (c == '\n') {
            ring_put(T.cur, T.curlen, T.line_type);
            T.curlen = 0;
            closed++;
            continue;
        }
        if ((uint8_t)c < 0x20 || c == 0x7f)      /* \r 等控制符丢弃 */
            continue;
        if (T.curlen >= TERM_LINE_MAX - 2) {
            /* 缓冲满：在最后一个空格处断行（保单词完整）；
             * 像素级换行由 label 的 WRAP 模式负责，这里不做字符预换行 */
            int cut = T.curlen;
            while (cut > 0 && T.cur[cut - 1] != ' ') cut--;
            if (cut == 0) cut = T.curlen;      /* 无空格则硬断 */
            char tail[TERM_LINE_MAX];
            int tail_len = T.curlen - cut;
            memcpy(tail, T.cur + cut, tail_len);
            T.curlen = cut;
            ring_put(T.cur, T.curlen, T.line_type);
            memcpy(T.cur, tail, tail_len);
            T.curlen = tail_len;
            closed++;
        }
        if (T.curlen < TERM_LINE_MAX - 1) T.cur[T.curlen++] = c;
    }
    /* 滚动锚定：回看历史时不被新内容拽回底部，新行数累计成提示 */
    if (T.scroll > 0) {
        T.scroll += closed;
        if (T.scroll > TERM_LINES_RING - 1) T.scroll = TERM_LINES_RING - 1;
        T.new_cnt += closed;
        if (T.new_cnt > 99) T.new_cnt = 99;
    } else {
        T.new_cnt = 0;
    }
    T.dirty = 1;                          /* key_timer 50ms 内统一重绘 */
}

/* 整行输出（type 决定行颜色：问题绿 / 系统灰 / 答案默认浅色） */
static void term_line(const char *s, uint8_t type)
{
    T.line_type = type;
    term_append(s, strlen(s));
    term_append("\n", 1);
}

static void term_line_sys(const char *s)
{
    term_line(s, LT_SYS);
}

/* ------------------------------------------------ 输入行 */
static void input_render(uint8_t cursor_on)
{
    static char buf[TERM_INPUT_MAX + 24];

    if (T.scroll > 0) {
        /* 回看历史中：提示滚动状态与期间新到内容数 */
        if (T.new_cnt)
            snprintf(buf, sizeof buf, "[^ %u up (+%u new)]", T.scroll, T.new_cnt);
        else
            snprintf(buf, sizeof buf, "[^ %u line%s up]",
                     T.scroll, T.scroll > 1 ? "s" : "");
    } else if (T.busy) {
        /* 生成中：闪烁光标即活动指示 */
        snprintf(buf, sizeof buf, "%c", cursor_on ? '_' : ' ');
    } else {
        /* 只显示尾部（单行像素宽度内），按 inlen 精确渲染 */
        const char *show = T.inbuf;
        int show_len = T.inlen;
        if (show_len > 30) { show += show_len - 30; show_len = 30; }
        snprintf(buf, sizeof buf, "> %.*s%c",
                 show_len, show, cursor_on ? '_' : ' ');
    }
    lv_label_set_text(lbl_in, buf);
}

static void cursor_timer_cb(lv_timer_t *t)
{
    static uint8_t flip = 0;
    flip ^= 1;
    input_render(flip);
}

/* ------------------------------------------------ 回车 → LLM */
static void submit_line(void)
{
    if (T.inlen == 0) return;
    T.inbuf[T.inlen] = '\0';

    if (T.busy) {
        term_line_sys("(busy: still generating, try later)");
        T.inlen = 0;
        return;
    }

    /* 问题回显（"> " 前缀区分） */
    char q[TERM_INPUT_MAX + 8];
    snprintf(q, sizeof q, "> %s", T.inbuf);

    int ret = llm_client_ask(T.inbuf);
    T.inlen = 0;
    if (ret == -2) {
        term_line_sys("(busy)");
        return;
    }
    if (ret != 0) {
        term_line_sys("(llm not ready: AP service offline?)");
        return;
    }
    term_line(q, LT_Q);
    T.line_type = LT_ANS;                     /* 此后流式答案/结尾空行按答案色 */
    T.busy = 1;                                     /* 答案流直接追加 */
#if CONFIG_LLMTERM_TTS
    tts_speak_reset();                        /* 打断上一段朗读，开始新答案 */
#endif
}

/* ------------------------------------------------ 输入轮询（worq.ui）
 * 整行（LLMKB1:L）与单键（LLMKB1:K）两路；↑↓（ESC+'A'/'B'）滚动历史。 */
static void key_timer_cb(lv_timer_t *t)
{
    static uint8_t tick4s;                  /* 50ms × 40 = 2s 刷新一次状态 */
    char line[128];

    if (T.dirty) {                          /* 流式输出统一冲刷（50ms 合并） */
        T.dirty = 0;
        ring_render();
    }

    while (net_kb_getline(line, sizeof line) == 0) {
        int n = strlen(line);
        if (n > TERM_INPUT_MAX) n = TERM_INPUT_MAX;
        memcpy(T.inbuf, line, n);
        T.inbuf[n] = '\0';
        T.inlen = n;
        submit_line();
        if (T.busy) break;                  /* 忙时丢弃后续行 */
    }

    int k;
    while ((k = net_kb_getkey()) >= 0) {
        if (T.esc_pending) {
            T.esc_pending = 0;
            if (k == 'A' && T.scroll < TERM_LINES_RING - 1) {
                T.scroll++;
                ring_render();
            } else if (k == 'B' && T.scroll > 0) {
                T.scroll--;
                if (T.scroll == 0) T.new_cnt = 0;   /* 回底，清新行计数 */
                ring_render();
            }
            continue;
        }
        if (k == 0x1b) {                     /* ↑↓ 转义序列首字节 */
            T.esc_pending = 1;
            continue;
        }
        if (k == '\n' || k == '\r') {
            submit_line();
        } else if (k == '\b' || k == 0x7f) {
            if (T.inlen > 0) T.inbuf[--T.inlen] = '\0';
        } else if (k >= 0x20 && k < 0x7f) {
            if (T.inlen < TERM_INPUT_MAX) {
                T.inbuf[T.inlen++] = (char)k;
                T.inbuf[T.inlen] = '\0';
            }
        }
        if (T.scroll) { T.scroll = 0; T.new_cnt = 0; ring_render(); }  /* 普通键回底 */
    }
    input_render(1);

    if (++tick4s >= 40) {
        tick4s = 0;
        lv_obj_set_style_bg_color(lbl_status,
            net_kb_peer_alive() ? lv_color_hex(COL_LIGHT_G)
                                : lv_color_hex(0x555555), 0);
    }
}

/* ------------------------------------------------ LLM 订阅（rpc_client 线程）
 * delta 拷入静态快照，按值投递到 worq.ui 追加（workq 会拷贝 arg） */
typedef struct {
    int evt;
    uint32_t len;
    char delta[96];
} llm_evt_snap_t;

static void llm_evt_worker(void *arg, uint32_t len)
{
    llm_evt_snap_t *e = arg;
    if (e->evt == LLM_CLIENT_EVT_PIECE || e->evt == LLM_CLIENT_EVT_DONE) {
        term_append(e->delta, e->len);
        if (e->evt == LLM_CLIENT_EVT_DONE) {
            term_append("\n\n", 2);              /* 闭合答案 + 问答对间空行 */
            T.busy = 0;
        }
    } else {   /* LLM_CLIENT_EVT_LOST */
        term_line_sys("(generate failed)");
        T.busy = 0;
    }
    input_render(1);
    (void)len;
}

static void on_llm_evt(void *user, int evt, const char *delta,
                       uint32_t dlen, uint32_t tokens, uint32_t ms)
{
    (void)user; (void)tokens; (void)ms;
    llm_evt_snap_t snap = { .evt = evt, .len = 0 };
    if (delta && dlen) {
        uint32_t n = dlen > sizeof snap.delta ? sizeof snap.delta : dlen;
        memcpy(snap.delta, delta, n);
        snap.len = n;
    }
    lisa_ui_invoke_ui_delayed(llm_evt_worker, &snap, sizeof snap, 0);
}

/* ------------------------------------------------ 跨线程系统提示（term_ui.h）
 * 非 worq.ui 线程（voice_asr 的 ptt/ebus 等）输出灰字提示行：快照经
 * workq 拷贝投递，与 llm_evt_snap 同模式。 */
static void hint_worker(void *arg, uint32_t len)
{
    term_line_sys((const char *)arg);
    input_render(1);
    (void)len;
}

void term_ui_sys_hint(const char *s)
{
    char snap[80];
    if (!s || !s[0]) return;
    snprintf(snap, sizeof snap, "%s", s);
    lisa_ui_invoke_ui_delayed(hint_worker, snap, strlen(snap) + 1, 0);
}

/* ------------------------------------------------ UI 构建（lisa_ui_init 内调用） */
int llmterm_ui_init(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG_SCREEN), 0);
    lv_obj_clean(scr);

    /* 标题栏底色条（先建，信号灯/标题叠在其上） */
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 240, TERM_TITLE_H);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COL_BG_TITLE), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 标题栏：macOS 信号灯 ● ● ● + 居中标题 */
    static const uint32_t lights[] = { COL_LIGHT_R, COL_LIGHT_Y, COL_LIGHT_G };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *dot = lv_obj_create(scr);
        lv_obj_set_size(dot, 9, 9);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(lights[i]), 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_align(dot, LV_ALIGN_TOP_LEFT, 8 + i * 14, 8);
    }

    lv_obj_t *lbl_title = lv_label_create(scr);
    lv_label_set_text(lbl_title, TERM_TITLE);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(COL_TITLE), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 4);

    /* 状态角标：键盘在线绿点 */
    lbl_status = lv_obj_create(scr);
    lv_obj_set_size(lbl_status, 6, 6);
    lv_obj_set_style_radius(lbl_status, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(lbl_status, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(lbl_status, 0, 0);
    lv_obj_set_style_pad_all(lbl_status, 0, 0);
    lv_obj_align(lbl_status, LV_ALIGN_TOP_RIGHT, -6, 9);

    /* 标题栏底部分隔线 */
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 240, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x484848), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_align(sep, LV_ALIGN_TOP_LEFT, 0, TERM_TITLE_H);

    /* 输出区：固定高度裁剪容器 + 底部对齐 label（自动滚屏） */
    cont_out = lv_obj_create(scr);
    lv_obj_set_size(cont_out, 240, 240 - TERM_TITLE_H - TERM_INPUT_H);
    lv_obj_align(cont_out, LV_ALIGN_TOP_LEFT, 0, TERM_TITLE_H);
    lv_obj_set_style_bg_color(cont_out, lv_color_hex(COL_BG_SCREEN), 0);
    lv_obj_set_style_border_width(cont_out, 0, 0);
    lv_obj_set_style_pad_all(cont_out, 0, 0);
    lv_obj_set_style_radius(cont_out, 0, 0);
    lv_obj_clear_flag(cont_out, LV_OBJ_FLAG_SCROLLABLE);   /* 裁剪不滚动 */

    lbl_out = lv_label_create(cont_out);
    lv_label_set_long_mode(lbl_out, LV_LABEL_LONG_WRAP);
    lv_label_set_recolor(lbl_out, true);          /* 行级 #RRGGBB 问答配色 */
    lv_label_set_recolor(lbl_out, true);    /* 行级着色：#RRGGBB 文本# */
    lv_obj_set_style_text_color(lbl_out, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(lbl_out, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl_out, 234);
    lv_obj_align(lbl_out, LV_ALIGN_BOTTOM_LEFT, 3, 0);     /* 最新行贴底 */

    /* 输入行 */
    lbl_in = lv_label_create(scr);
    lv_label_set_long_mode(lbl_in, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(lbl_in, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(lbl_in, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl_in, 236);
    lv_obj_align(lbl_in, LV_ALIGN_BOTTOM_LEFT, 3, -1);

    memset(&T, 0, sizeof T);
    term_line_sys("LLM terminal ready.");
    term_line_sys("type + Enter to ask (english)");
    term_line_sys("up/down arrows scroll history");

    ring_render();                          /* 欢迎语立即上屏（不等首拍定时器） */
    input_render(1);
    lv_timer_create(key_timer_cb, 50, NULL);
    lv_timer_create(cursor_timer_cb, 350, NULL);
    return 0;
}

/* ------------------------------------------------ 模块总入口（main 启动期） */
int llmterm_init(void)
{
    int ret = net_kb_init();
    if (ret != 0) LISA_LOGE(TAG, "net_kb init failed");

    ret = llm_client_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "llm client init failed");
        return ret;
    }
    if (llm_client_subscribe(on_llm_evt, NULL) != 0) {
        LISA_LOGE(TAG, "llm subscribe failed");
        return -1;
    }
#if CONFIG_LLMTERM_TTS
    if (tts_speak_init() != 0) {
        LISA_LOGE(TAG, "tts speak init failed");   /* 不阻断终端 */
    }
#endif
#if CONFIG_LLMTERM_ASR
    {
        extern int voice_asr_init(void);
        if (voice_asr_init() != 0) {
            LISA_LOGE(TAG, "voice asr init failed");  /* 不阻断终端 */
        }
    }
#endif
    LISA_LOGI(TAG, "llmterm ready");
    return 0;
}
