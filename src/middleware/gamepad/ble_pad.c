/*
 * ble_pad.c - CodexPad-S10 BLE 手柄驱动实现（Central）
 *
 * 线程模型（对齐 SDK 官方 central demo samples/bluetooth/ble/central/client）：
 *   - BLE 回调（scan_report / 各类 activity / service / notification ...）在
 *     BT task 上下文被调用，只做「拷贝 + 入队」；
 *   - 独立 app task 消费事件队列，执行状态机，并调用线程安全的
 *     lisa_ble_*（内部经 BT task 事件队列投递）与 lisa_ble_client_*（GATT 客户端）。
 *   - 绝不在 BT task 回调里直接发起扫描/连接（历史死机根因：在 BT task 内
 *     调用 lisa_ble_connect 会把事件重新投递回同一任务队列）。
 *
 * 协议常量见 codex_pad_s10/INTEGRATION_GUIDE.zh-CN.md §3.6。
 *
 * shell: blepad scan|on|off|status
 */
#define TAG "ble_pad"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "lisa_log.h"
#include "lisa_thread.h"
#include "shell.h"

#include "gamepad.h"
#include "gamepad_internal.h"
#include "gamepad_proto.h"

#include "ble_pad.h"

/* L1/L2 唤醒相关：与设备自身按键单击唤醒/结束唤醒走同一条消息（see app_button.c） */
#include "voice_msg.h"
#include "voice_cloud.h"        /* voice_cloud_is_session_active (L2 退出语音模式前判断) */
#if CONFIG_WIFI
#include "sys_wifi.h"
#endif

/* SDK 官方 BLE 接口 */
#include "lisa_ble_api.h"       /* 扫描/连接（线程安全的事件封装） */
#include "lisa_ble_client.h"    /* GATT 客户端（Central） */
#include "ble_gap.h"            /* GAP 常量与 gap_bdaddr_t */

/* ================================================================== */
/* CodexPad 协议常量                                                    */
/* ================================================================== */

#define CPAD_SVC_INPUTS   0xFFA0
#define CPAD_CHAR_INPUTS  0xFFA1
#define CPAD_DESC_CCCD    0x2902

#define CPAD_FRAME_LEN    8

/* 按键位（INTEGRATION_GUIDE §3.6） */
enum {
    CPAD_UP     = 1u << 0,
    CPAD_DOWN   = 1u << 1,
    CPAD_LEFT   = 1u << 2,
    CPAD_RIGHT  = 1u << 3,
    CPAD_X      = 1u << 4,   /* 方块/Square */
    CPAD_Y      = 1u << 5,   /* 三角/Triangle */
    CPAD_A      = 1u << 6,   /* 叉/Cross（手柄物理键名，非协议 a） */
    CPAD_B      = 1u << 7,   /* 圆/Circle（手柄物理键名，非协议 b） */
    CPAD_L1     = 1u << 8,
    CPAD_L2     = 1u << 9,
    CPAD_L3     = 1u << 10,
    CPAD_R1     = 1u << 11,
    CPAD_R2     = 1u << 12,
    CPAD_R3     = 1u << 13,
    CPAD_SELECT = 1u << 14,
    CPAD_START  = 1u << 15,
    CPAD_HOME   = 1u << 16,
};

/* 扫描参数：通用发现（active，能收 SCAN_RSP）。interval/window 单位 0.625ms。
 * ⚠️ 必须满足 BLE 规范 win <= intv，且不能超过 SDK 路径能承载的范围：
 * lisa_ble_scan_param() 经 btos 事件把这两个值送到 BT task，字段宽度受限，
 * 一旦截断就可能变成 win > intv 的非法组合 —— 现象是扫描活动能起来
 * （active=1/status=0）但一条广播报告都不上报。 */
#define CPAD_SCAN_INTV  160   /* 100ms */
#define CPAD_SCAN_WIN   80    /* 50ms  */
#define CPAD_SCAN_PHY   0x05  /* 1M PHY(bit0) + active(bit2)，同 SDK 默认 */

_Static_assert(CPAD_SCAN_INTV > 0 && CPAD_SCAN_INTV <= 0xFFFFU,
               "scan interval out of range");
_Static_assert(CPAD_SCAN_WIN > 0 && CPAD_SCAN_WIN <= 0xFFFFU,
               "scan window out of range");
_Static_assert(CPAD_SCAN_WIN <= CPAD_SCAN_INTV,
               "scan window must not exceed interval");

/* 连接参数：官方库实测值 7.5~10ms / latency 5 / timeout 1s */
#define CPAD_CONN_INTV_MIN  6
#define CPAD_CONN_INTV_MAX  8
#define CPAD_CONN_LATENCY   5
#define CPAD_CONN_TIMEOUT   100

/* 自动档按键掩码（Start+A 按住 ≥1s 触发连接，官方推荐组合） */
#define CPAD_CONNECT_MASK  (CPAD_START | CPAD_A)

/* 左摇杆（十字键下方那颗）→ 方向键。
 *
 * 纯轴向判定的角度死区取 15°：偏离轴向 < 15° 视为纯轴向，因此
 *   0°  / 90° / 180° / 270°  → 单方向（左/右/上/下）
 *   45° / 135° / 225° / 315° → 双方向（左上/右上/左下/右下）
 * X/Y 两轴各自独立判定再按位或，对角线天然成立，不需要单独的表。
 * 死区 = 128 * sin(15°) ≈ 33，即某轴偏移量小于它就不算该轴向生效。
 * 文档口径：值小 = 左/下，值大 = 右/上（若实机上下相反，只翻 LY 两行）。 */
#define CPAD_STICK_CENTER    128
#define CPAD_STICK_DEADZONE  33
_Static_assert(CPAD_STICK_DEADZONE > 0 &&
               CPAD_STICK_DEADZONE < CPAD_STICK_CENTER,
               "stick deadzone out of range");

/* 按住不放的保活重发间隔：手柄是事件驱动上报（按键状态不变就不发帧），
 * 这里周期性把最近一次位图重新写回 gamepad 层，确保「按住」在小应用侧
 * 持续有效（也覆盖输入层被其它手柄通道瞬间清零的情况）。 */
#define CPAD_KEEPALIVE_MS  250

/* 只凭名字命中时的自动连接阈值（连续出现次数） */
#define CPAD_NAME_CONNECT_HITS 3

/* 状态看门狗：连接/GATT 阶段卡住就退回扫描 */
#define CPAD_CONNECT_TIMEOUT_MS 8000
#define CPAD_GATT_TIMEOUT_MS    8000

/* 开机自启时 BLE 栈可能尚未就绪（ipc 就绪后才 lisa_bluetooth_init），
 * client init 失败后按此间隔重试，直到栈就绪。 */
#define CPAD_INIT_RETRY_MS      1000

/* ================================================================== */
/* 驱动状态                                                             */
/* ================================================================== */

typedef enum {
    PAD_IDLE = 0,
    PAD_SCANNING,
    PAD_CONNECTING,
    PAD_DISCOVERING,
    PAD_SUBSCRIBING,
    PAD_RUNNING,
} pad_state_t;

typedef struct {
    volatile pad_state_t state;
    volatile bool started;
    volatile uint8_t conidx;
    gap_bdaddr_t peer;
    char peer_str[18];

    uint16_t svc_shdl, svc_ehdl;
    uint16_t val_hdl;
    uint16_t cccd_hdl;

    volatile uint32_t frames;
    volatile uint32_t notify_count;   /* 收到的 Notify 总数（不判长度/句柄） */
    volatile uint32_t scan_matches;
    volatile uint32_t scan_reports;   /* 收到的广播报告总数（诊断用） */
    volatile int8_t last_rssi;
    volatile uint8_t dbg_buttons, dbg_lx, dbg_ly;
    volatile uint32_t dbg_raw;        /* 最近一帧原始 u32 按键位图 */
    volatile uint16_t last_mask;      /* 最近一次写入 gamepad 层的线上位图 */
    volatile uint16_t last_stick_dir; /* 上次摇杆方向（变化才打印） */
    volatile uint32_t last_stick_btn; /* 上次摇杆按键 L3/R3（变化才打印） */
    volatile bool l1_down;            /* L1 上一帧状态（上升沿触发唤醒） */
    volatile bool l2_down;            /* L2 上一帧状态（上升沿退出语音模式） */
    TickType_t keepalive_at;          /* 按住保活重发时刻 */
    volatile bool raw_log;            /* shell: blepad raw 开关 */
    volatile bool stick_log;          /* shell: blepad stick 开关（默认关） */
    volatile bool scan_diag;          /* shell: blepad diag 开关（默认关） */
    TickType_t connect_deadline;      /* 连接/GATT 阶段看门狗 */
    TickType_t init_retry_at;         /* 开机 BLE 未就绪时重试 client init */
    uint8_t    init_retries;          /* 重试次数（仅首次失败打日志） */
} pad_t;

static pad_t s_pad;

/* ================================================================== */
/* 事件队列 / app task                                                  */
/* ================================================================== */

#define PAD_EVQ_LEN 16
#define PAD_ADV_DATA_MAX 31
#define PAD_NOTIFY_MAX 16

typedef enum {
    EV_CMD_START = 1,
    EV_CMD_STOP,
    EV_CMD_SCAN,
    EV_CMD_CONNECT,      /* 手动连接最近一次扫描到的 CodexPad */
    EV_SCAN_REPORT,
    EV_SCAN_ACTIVITY,
    EV_CONN_ACTIVITY,
    EV_CONN_FAILED,
    EV_CONN_PARAMS,
    EV_CONNECTED,
    EV_DISCONNECTED,
    EV_DISCOVER_COMPLETE,
    EV_SERVICE,
    EV_WRITE_COMPLETE,
    EV_NOTIFICATION,
    EV_SET_RAW_LOG,      /* status: 1=on 0=off */
    EV_SET_SCAN_DIAG,    /* status: 1=on 0=off，每秒一条广播流量总览 */
    EV_SET_STICK_LOG,    /* status: 1=on 0=off，摇杆方向/摇杆按键事件 */
} pad_evt_type_t;

typedef struct {
    uint8_t type;
    union {
        struct { uint8_t addr_type; uint8_t addr[6]; int8_t rssi;
                 uint8_t flags; uint8_t len; uint8_t data[PAD_ADV_DATA_MAX]; } scan;
        struct { bool active; uint8_t id; uint16_t status; } actv;
        uint16_t status;
        struct { uint8_t conidx; uint16_t interval, latency, timeout; } params;
        struct { uint8_t conidx; uint16_t conhdl; } connected;
        struct { uint8_t conidx; uint16_t reason; } disconnected;
        struct { uint8_t conidx; uint16_t status; } procedure;
        struct { uint16_t val_hdl; uint16_t cccd_hdl; } service;
        struct { uint8_t conidx; uint16_t hdl; uint16_t status; } write;
        struct { uint8_t conidx; uint16_t hdl; uint8_t len;
                 uint8_t data[PAD_NOTIFY_MAX]; } notify;
    } u;
} pad_evt_t;

static QueueHandle_t s_evq;
static lisa_thread_t *s_task;

static void pad_send_cmd(uint8_t cmd);   /* 定义在文件后部，看门狗会用到 */

static void pad_post(const pad_evt_t *ev) {
    if (!s_evq) return;
    (void)xQueueSend(s_evq, ev, 0);
}

/* ================================================================== */
/* 工具                                                                 */
/* ================================================================== */

static void peer_to_str(const gap_bdaddr_t *a, char *out, size_t n) {
    snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X",
             a->addr[5], a->addr[4], a->addr[3], a->addr[2], a->addr[1], a->addr[0]);
}

/* CodexPad 按键位图 → 线上手柄位图 (gamepad_proto.h)
 *
 * 注意 CPAD_A/CPAD_B 是手柄物理键名（叉/Cross、圆/Circle），与协议里的
 * a/b 不是同一个概念。按实机手感，叉当确认、圆当返回（需换回改这两行即可）。
 * 这些位会被 gamepad_input 翻译成小应用按键: 叉/START/十字键 → 移动与确认,
 * 圆 → 返回, SELECT → 设置。 */
static uint16_t cpad_buttons_to_mask(uint32_t b) {
    uint16_t m = 0;
    if (b & CPAD_UP)     m |= GAMEPAD_KEY_UP;
    if (b & CPAD_DOWN)   m |= GAMEPAD_KEY_DOWN;
    if (b & CPAD_LEFT)   m |= GAMEPAD_KEY_LEFT;
    if (b & CPAD_RIGHT)  m |= GAMEPAD_KEY_RIGHT;
    if (b & CPAD_A)      m |= GAMEPAD_KEY_A;   /* 叉/Cross → 确认 */
    if (b & CPAD_B)      m |= GAMEPAD_KEY_B;   /* 圆/Circle → 返回 */
    if (b & CPAD_SELECT) m |= GAMEPAD_KEY_SELECT;
    if (b & CPAD_START)  m |= GAMEPAD_KEY_START;
    return m;
}

/*
 * 摇杆 → 方向键（十字键）。
 *
 * 十字键下方那颗左摇杆（LX/LY，中心 0x80）在桌面导航上无模拟量语义，直接
 * 按阈值当方向键用：值 < 中心-死区 判左/下，> 中心+死区 判右/上，中间
 * 死区不产生输入（手柄静止时输出接近但不必等于 0x80，没有死区会漂移）。
 * 方向键本身按下时与摇杆结果按位或叠加，二者任一即可触发。
 */
static uint16_t cpad_stick_to_dpad(uint8_t lx, uint8_t ly) {
    uint16_t m = 0;
    const int center = CPAD_STICK_CENTER;
    const int dz = CPAD_STICK_DEADZONE;

    if ((int)lx < center - dz)      m |= GAMEPAD_KEY_LEFT;
    else if ((int)lx > center + dz) m |= GAMEPAD_KEY_RIGHT;

    /* 文档口径: 值小 = 下, 值大 = 上 */
    if ((int)ly < center - dz)      m |= GAMEPAD_KEY_DOWN;
    else if ((int)ly > center + dz) m |= GAMEPAD_KEY_UP;
    return m;
}

/* 手柄 L1：触发一次唤醒（等同设备按键单击唤醒）。
 *
 * 复用设备自身按键的通路：先退 Wi-Fi 省电（待机时是最大监听间隔，不退的话
 * 出站请求要等下一个 beacon），再发布唤醒词消息。关键词必须与
 * apps/arcs-mini/button/app_button.c 一致——消费端 voice_wakeup_keyword()
 * 会先做 is_valid_keyword() 校验，字面量不对会被静默丢弃。闸门（工作模式、
 * 云端可用性、会话阻塞）全在消费端，这里不做判断。 */
static void pad_trigger_wakeup(void) {
    static const char keyword[] = "xiao ling xiao ling";

#if CONFIG_WIFI
    int ret = sys_wifi_set_standby_power_save(false);
    if (ret != 0) {
        LISA_LOGW(TAG, "WiFi standby resume before pad wake failed: %d", ret);
    }
#endif
    /* 本分支没有独占 audio0 的游戏 —— 小应用的蜂鸣走 app_player, 与语音链路
     * 同一条共享 16kHz 流, 唤醒时由播放器自身的焦点策略处理, 不需要显式让出。 */

    LISA_LOGI(TAG, "L1: wakeup trigger");
    voice_msg_pub(VOICE_MSG_WAKEUP_KEYWORD, (void *)keyword, sizeof(keyword));
}

/* 手柄 L2：退出语音模式，回到「唤醒等待」。
 *
 * 与设备按键的「结束当前唤醒」是同一个动作 —— 发会话中断消息，
 * 消费端 voice_cloud_session_interrupt() 会打断正在播报的回复、
 * 停止云端会话并弹出 VOICE_SESSION 意图，随后设备回到只等唤醒词的状态。
 * 没有进行中的会话时不做任何事（避免无意义的 stop 与告警）。 */
static void pad_trigger_voice_exit(void) {
    if (!voice_cloud_is_session_active()) {
        LISA_LOGI(TAG, "L2: no active voice session, ignore");
        return;
    }
    LISA_LOGI(TAG, "L2: exit voice mode");
    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_INTERRUPT, NULL, 0);
}

/* 摇杆方向位图 → 可读字符串（写入调用方缓冲：app task 与 shell task 都会
 * 调用，不能用共享静态缓冲）。最长 "UP+DOWN+LEFT+RIGHT"(18) + NUL，
 * 调用方给 >= 24 字节即可。 */
static void pad_dir_str(uint16_t dir, char *out, size_t n) {
    static const struct { uint16_t bit; const char *name; } k_names[] = {
        { GAMEPAD_KEY_UP,    "UP"    },
        { GAMEPAD_KEY_DOWN,  "DOWN"  },
        { GAMEPAD_KEY_LEFT,  "LEFT"  },
        { GAMEPAD_KEY_RIGHT, "RIGHT" },
    };
    size_t w = 0;

    if (out == NULL || n == 0) return;
    if (dir == 0) {
        snprintf(out, n, "CENTER");
        return;
    }
    out[0] = 0;
    for (size_t i = 0; i < sizeof(k_names) / sizeof(k_names[0]); i++) {
        if (!(dir & k_names[i].bit)) continue;
        if (w + 1 >= n) break;              /* 无空间，安全截断 */
        int r = snprintf(out + w, n - w, "%s%s", w ? "+" : "", k_names[i].name);
        if (r > 0) w += (size_t)r;
    }
}

/* 8 字节输入帧在 EV_NOTIFICATION 里按「按键位图 | 摇杆方向」合成线上位图
 * （见该处），这里不再单独封装，避免逻辑分散两处。 */

/*
 * 广播数据（AD structure 序列）解析。
 *
 * CodexPad 的名字在 ADV 包、Manufacturer Data（按键位图）在 SCAN RESPONSE
 * 包，两包按对端地址配对合并后才是一次完整广播。缓存 8 个最近地址，
 * adv/scan_rsp 分别更新各自字段。
 */

#define CPAD_PEER_CACHE_NUM 8

typedef struct {
    gap_bdaddr_t addr;
    bool used;
    bool name_ok;
    bool mfg_ok;
    uint8_t fw_major;
    uint32_t button_state;
    uint8_t duration_s;
    int8_t rssi;
    TickType_t update_tick;
} cpad_peer_t;

static cpad_peer_t s_peers[CPAD_PEER_CACHE_NUM];

static cpad_peer_t *cpad_peer_slot(const gap_bdaddr_t *addr) {
    cpad_peer_t *oldest = &s_peers[0];
    for (int i = 0; i < CPAD_PEER_CACHE_NUM; i++) {
        if (s_peers[i].used &&
            memcmp(s_peers[i].addr.addr, addr->addr, 6) == 0) {
            return &s_peers[i];
        }
        if (!s_peers[i].used) oldest = &s_peers[i];
    }
    for (int i = 0; i < CPAD_PEER_CACHE_NUM; i++) {
        if (s_peers[i].update_tick < oldest->update_tick) oldest = &s_peers[i];
    }
    memset(oldest, 0, sizeof(*oldest));
    oldest->used = true;
    oldest->addr = *addr;
    return oldest;
}

/* 单包 AD 结构解析：名字前缀 / MFG 校验，命中字段更新到 slot */
static void cpad_adv_parse(const uint8_t *data, uint8_t len, cpad_peer_t *pe) {
    uint8_t i = 0;
    while (i + 1 < len) {
        uint8_t ad_len = data[i];
        if (ad_len == 0 || i + 1 + ad_len > len) break;
        uint8_t ad_type = data[i + 1];
        const uint8_t *p = &data[i + 2];
        uint8_t plen = ad_len - 1;

        if ((ad_type == 0x09 || ad_type == 0x08) && plen >= 9) {
            char name[16] = {0};
            size_t cp = plen < 15 ? plen : 15;
            memcpy(name, p, cp);
            if (strncmp(name, "CodexPad-", 9) == 0) pe->name_ok = true;
        } else if (ad_type == 0xFF && plen >= 16) {
            /* [0..1] company id 0xFFFF + [2..9] "CodexPad" + [10..12] ver
             * + [13..16] button u32 LE + [17] duration（可能截断） */
            uint16_t cid = (uint16_t)(p[0] | (p[1] << 8));
            if (cid == 0xFFFF && memcmp(&p[2], "CodexPad", 8) == 0) {
                pe->mfg_ok = true;
                pe->fw_major = p[10];
                pe->button_state = (uint32_t)p[13] | ((uint32_t)p[14] << 8) |
                                   ((uint32_t)p[15] << 16) | ((uint32_t)p[16] << 24);
                pe->duration_s = plen >= 18 ? p[17] : 0;
            }
        }
        i += 1 + ad_len;
    }
}

/* ================================================================== */
/* lisa_ble_client 回调（BT task 上下文，只做拷贝+入队）                 */
/* ================================================================== */

static uint16_t uuid16_get(const lisa_ble_uuid_t *u) {
    if (u->type != LISA_BLE_UUID_16) return 0;
    return (uint16_t)(u->value[0] | (u->value[1] << 8));
}

/* 广播报告流量总览（默认关闭，`blepad diag` 打开）。
 * 放在 BT task 上下文执行是为了能看清「一包都收不到」这种情况；
 * 打开时会同步执行 LISA_LOGI（UART 输出），故默认必须关闭。 */
static void pad_log_scan_report(const pad_evt_t *ev) {
    char nm[24] = {0};
    uint8_t j = 0;
    while (j + 1 < ev->u.scan.len) {
        uint8_t ad_len = ev->u.scan.data[j];
        if (ad_len == 0 || j + 1 + ad_len > ev->u.scan.len) break;
        if ((ev->u.scan.data[j + 1] == 0x09 || ev->u.scan.data[j + 1] == 0x08) &&
            ad_len > 1) {
            uint8_t cp = (ad_len - 1) < 23 ? (ad_len - 1) : 23;
            memcpy(nm, &ev->u.scan.data[j + 2], cp);
            nm[cp] = 0;
            break;
        }
        j += 1 + ad_len;
    }
    gap_bdaddr_t addr;
    char ps[18];
    memset(&addr, 0, sizeof(addr));
    memcpy(addr.addr, ev->u.scan.addr, 6);
    addr.addr_type = ev->u.scan.addr_type;
    peer_to_str(&addr, ps, sizeof(ps));
    LISA_LOGI(TAG, "scan: total=%u last=%s rssi=%d type=%u name=%s",
              (unsigned)s_pad.scan_reports, ps, ev->u.scan.rssi,
              (unsigned)(ev->u.scan.flags & 0x07), nm[0] ? nm : "-");
}

static void pad_on_scan_report(const lisa_ble_scan_report_t *r, void *ud) {
    pad_evt_t ev;
    (void)ud;
    if (!r) return;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_SCAN_REPORT;
    ev.u.scan.addr_type = r->addr.addr_type;
    memcpy(ev.u.scan.addr, r->addr.addr, 6);
    ev.u.scan.rssi = r->rssi;
    ev.u.scan.flags = r->flags;
    size_t sz = r->length;
    if (sz > PAD_ADV_DATA_MAX) sz = PAD_ADV_DATA_MAX;   /* 设备端只留前 31B */
    ev.u.scan.len = (uint8_t)sz;
    if (sz && r->data) memcpy(ev.u.scan.data, r->data, sz);
    /* 计数与诊断都在 BT task 上下文完成：计数不判 state（扫描活动可能
     * 已停但报告仍在途），诊断默认关闭、零开销。 */
    s_pad.scan_reports++;
    if (s_pad.scan_diag) pad_log_scan_report(&ev);
    pad_post(&ev);
}

static void pad_on_scan_activity(bool active, uint8_t scan_id, uint16_t status,
                                 void *ud) {
    pad_evt_t ev;
    (void)ud;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_SCAN_ACTIVITY;
    ev.u.actv.active = active;
    ev.u.actv.id = scan_id;
    ev.u.actv.status = status;
    pad_post(&ev);
}

static void pad_on_conn_activity(bool active, uint8_t activity_id, uint16_t status,
                                 void *ud) {
    pad_evt_t ev;
    (void)ud;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_CONN_ACTIVITY;
    ev.u.actv.active = active;
    ev.u.actv.id = activity_id;
    ev.u.actv.status = status;
    pad_post(&ev);
}

static void pad_on_conn_failed(uint16_t status, void *ud) {
    pad_evt_t ev;
    (void)ud;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_CONN_FAILED;
    ev.u.status = status;
    pad_post(&ev);
}

static void pad_on_conn_params(const lisa_ble_conn_params_t *p, void *ud) {
    pad_evt_t ev;
    (void)ud;
    if (!p) return;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_CONN_PARAMS;
    ev.u.params.conidx = p->conidx;
    ev.u.params.interval = p->interval;
    ev.u.params.latency = p->latency;
    ev.u.params.timeout = p->supervision_timeout;
    pad_post(&ev);
}

/* 服务发现：从属性表里提取 0xFFA1 特征的 value handle 及其 CCCD。
 *
 * ⚠️ value handle 与 CCCD 必须来自同一个特征：手柄有多个服务、每个都带
 * 自己的 0x2902，如果把「任意服务的 0x2902」当成 0xFFA1 的 CCCD，就会
 * 订阅到别的特征上——CCCD 写成功（status=0）但 0xFFA1 永远不发 Notify。
 * 所以：先在本服务里定位 0xFFA1，再取它之后（handle > val）的第一个
 * 0x2902 作为它的 CCCD；本服务里没有 0xFFA1 就整包丢弃。 */
static void pad_on_service(uint8_t conidx, const lisa_ble_gatt_service_t *svc,
                           void *ud) {
    pad_evt_t ev;
    (void)ud;
    (void)conidx;
    if (!svc) return;

    uint16_t val = 0, cccd = 0;
    for (uint8_t k = 0; k < svc->attribute_count; k++) {
        const lisa_ble_gatt_attribute_t *a = &svc->attributes[k];
        if (a->kind == LISA_BLE_GATT_ATTRIBUTE_CHARACTERISTIC &&
            uuid16_get(&a->uuid) == CPAD_CHAR_INPUTS) {
            val = a->value_handle;
            break;
        }
    }
    if (val == 0) {
        /* 本服务不含 0xFFA1，与目标无关 */
        LISA_LOGI(TAG, "svc 0x%04X-0x%04X attrs=%u: no 0xFFA1 (skip)",
                  svc->start_handle, svc->end_handle, svc->attribute_count);
        return;
    }
    for (uint8_t k = 0; k < svc->attribute_count; k++) {
        const lisa_ble_gatt_attribute_t *a = &svc->attributes[k];
        if (a->kind == LISA_BLE_GATT_ATTRIBUTE_DESCRIPTOR &&
            uuid16_get(&a->uuid) == CPAD_DESC_CCCD && a->handle > val) {
            cccd = a->handle;
            break;
        }
    }

    memset(&ev, 0, sizeof(ev));
    ev.type = EV_SERVICE;
    ev.u.service.val_hdl = val;
    ev.u.service.cccd_hdl = cccd;
    pad_post(&ev);
    LISA_LOGI(TAG, "0xFFA1 found in svc 0x%04X-0x%04X: val=0x%04X cccd=0x%04X",
              svc->start_handle, svc->end_handle, val, cccd);
}

static void pad_on_discover_complete(uint8_t conidx, uint16_t status, void *ud) {
    pad_evt_t ev;
    (void)ud;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_DISCOVER_COMPLETE;
    ev.u.procedure.conidx = conidx;
    ev.u.procedure.status = status;
    pad_post(&ev);
}

static void pad_on_write_complete(uint8_t conidx, uint16_t handle, uint16_t status,
                                  void *ud) {
    pad_evt_t ev;
    (void)ud;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_WRITE_COMPLETE;
    ev.u.write.conidx = conidx;
    ev.u.write.hdl = handle;
    ev.u.write.status = status;
    pad_post(&ev);
}

static void pad_on_notification(uint8_t conidx, uint16_t handle,
                                const uint8_t *value, uint16_t length,
                                bool indication, void *ud) {
    pad_evt_t ev;
    (void)ud;
    (void)indication;
    if (!value) return;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_NOTIFICATION;
    ev.u.notify.conidx = conidx;
    ev.u.notify.hdl = handle;
    uint16_t len = length > PAD_NOTIFY_MAX ? PAD_NOTIFY_MAX : length;
    ev.u.notify.len = (uint8_t)len;
    memcpy(ev.u.notify.data, value, len);
    pad_post(&ev);
}

static const lisa_ble_client_callbacks_t s_pad_cb = {
    .scan_report = pad_on_scan_report,
    .scan_activity = pad_on_scan_activity,
    .connection_activity = pad_on_conn_activity,
    .conn_params = pad_on_conn_params,
    .connection_failed = pad_on_conn_failed,
    .service = pad_on_service,
    .discover_complete = pad_on_discover_complete,
    .read_complete = NULL,
    .write_complete = pad_on_write_complete,
    .notification = pad_on_notification,
};

/* ================================================================== */
/* 连接/断开（由 app_ble_common.c 的 weak 钩子转发，BT task 上下文）     */
/* ================================================================== */

void ble_pad_gap_connected(uint8_t conidx, const gap_bdaddr_t *peer_addr) {
    pad_evt_t ev;
    (void)peer_addr;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_CONNECTED;
    ev.u.connected.conidx = conidx;
    pad_post(&ev);
}

void ble_pad_gap_disconnected(uint8_t conidx, uint16_t reason) {
    pad_evt_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_DISCONNECTED;
    ev.u.disconnected.conidx = conidx;
    ev.u.disconnected.reason = reason;
    pad_post(&ev);
}

/* ================================================================== */
/* 状态机（app task 上下文）                                             */
/* ================================================================== */

static void pad_scan_start(void) {
    LISA_LOGI(TAG, "scan start");
    lisa_ble_scan_param(GAPM_SCAN_TYPE_GEN_DISC, CPAD_SCAN_PHY,
                        CPAD_SCAN_INTV, CPAD_SCAN_WIN);
    s_pad.state = PAD_SCANNING;
}

/* lisa_ble_* 的返回值语义（lisa_ble_api_local.c:bt_send_event）：
 *   1   (pdTRUE)  事件投递成功
 *   0   (pdFALSE) btos 事件队列发送失败
 *   0xff         malloc 失败
 * 即「非 0 非 0xff 才算成功」，不能用 `ret != 0` 判失败。 */
static bool lisa_send_ok(int ret) {
    return ret != 0 && ret != 0xff;
}

/* 连接已发现的 CodexPad 对端（须先扫描到 peer 地址） */
static int pad_connect_peer(void) {
    bool all_zero = true;
    for (int k = 0; k < 6; k++) {
        if (s_pad.peer.addr[k] != 0) { all_zero = false; break; }
    }
    if (all_zero) {
        LISA_LOGW(TAG, "connect: no peer seen yet");
        return -1;
    }

    peer_to_str(&s_pad.peer, s_pad.peer_str, sizeof(s_pad.peer_str));
    lisa_ble_addr_t a;
    memcpy(a.addr, s_pad.peer.addr, 6);
    a.addr_type = s_pad.peer.addr_type;
    int ret = lisa_ble_connect(&a, CPAD_SCAN_PHY,
                               CPAD_CONN_INTV_MIN, CPAD_CONN_INTV_MAX,
                               CPAD_CONN_LATENCY, CPAD_CONN_TIMEOUT);
    if (!lisa_send_ok(ret)) {
        LISA_LOGE(TAG, "connect event send failed ret=%d", ret);
        return -1;
    }
    LISA_LOGI(TAG, "connecting %s (intv %u-%u lat %u to %u)",
              s_pad.peer_str, CPAD_CONN_INTV_MIN, CPAD_CONN_INTV_MAX,
              CPAD_CONN_LATENCY, CPAD_CONN_TIMEOUT);
    /* 发起成功：等 EV_CONNECTED / EV_CONN_FAILED，并起看门狗防挂死 */
    s_pad.state = PAD_CONNECTING;
    s_pad.connect_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CPAD_CONNECT_TIMEOUT_MS);
    return 0;
}

static void pad_discover_start(uint8_t conidx) {
    s_pad.svc_shdl = s_pad.svc_ehdl = s_pad.val_hdl = s_pad.cccd_hdl = 0;
    int ret = lisa_ble_client_discover_all(conidx);
    LISA_LOGI(TAG, "discover_all ret=%d conidx=%u", ret, conidx);
    if (ret != 0) {
        LISA_LOGE(TAG, "discover start failed");
        lisa_ble_disconnect(conidx, 0x13);
        return;
    }
    s_pad.state = PAD_DISCOVERING;
    s_pad.connect_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CPAD_GATT_TIMEOUT_MS);
}

static void pad_subscribe(uint8_t conidx) {
    int ret = lisa_ble_client_subscribe(conidx, s_pad.val_hdl, s_pad.cccd_hdl, true);
    LISA_LOGI(TAG, "subscribe val=0x%04X cccd=0x%04X ret=%d",
              s_pad.val_hdl, s_pad.cccd_hdl, ret);
    if (ret != 0) {
        LISA_LOGE(TAG, "subscribe failed, disconnect");
        lisa_ble_disconnect(conidx, 0x13);
        return;
    }
    s_pad.state = PAD_SUBSCRIBING;
    s_pad.connect_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CPAD_GATT_TIMEOUT_MS);
}

static void pad_handle_scan_report(const pad_evt_t *ev) {
    if (!s_pad.started || s_pad.state != PAD_SCANNING) return;

    gap_bdaddr_t addr;
    memset(&addr, 0, sizeof(addr));
    memcpy(addr.addr, ev->u.scan.addr, 6);
    addr.addr_type = ev->u.scan.addr_type;

    cpad_peer_t *pe = cpad_peer_slot(&addr);
    pe->rssi = ev->u.scan.rssi;
    pe->update_tick = xTaskGetTickCount();
    cpad_adv_parse(ev->u.scan.data, ev->u.scan.len, pe);

    if (!pe->name_ok) return;

    s_pad.scan_matches++;
    s_pad.last_rssi = ev->u.scan.rssi;
    s_pad.peer = pe->addr;

    peer_to_str(&pe->addr, s_pad.peer_str, sizeof(s_pad.peer_str));
    LISA_LOGI(TAG, "adv: %s rssi=%d mfg=%d fw=%u btn=0x%X dur=%u hits=%u",
              s_pad.peer_str, pe->rssi, pe->mfg_ok, pe->fw_major,
              pe->button_state, pe->duration_s, s_pad.scan_matches);

    /* 自动连接：优先 Start+A 掩码（需 MFG），退化为名字命中若干次 */
    bool want = false;
    if (pe->mfg_ok && pe->button_state == CPAD_CONNECT_MASK && pe->duration_s >= 1) {
        LISA_LOGI(TAG, "trigger: Start+A arming detected");
        want = true;
    } else if (s_pad.scan_matches >= CPAD_NAME_CONNECT_HITS) {
        LISA_LOGI(TAG, "trigger: name hits >= %u", CPAD_NAME_CONNECT_HITS);
        want = true;
    }
    if (!want) return;

    pad_connect_peer();
}

static void pad_task(void *arg) {
    pad_evt_t ev;
    (void)arg;

    while (1) {
        /* 1s 超时：既处理事件，也驱动下面的看门狗 */
        if (xQueueReceive(s_evq, &ev, pdMS_TO_TICKS(1000)) != pdPASS) {
            goto watchdog;
        }

        switch (ev.type) {
        case EV_CMD_START:
            if (!s_pad.started) {
                s_pad.started = true;
                s_pad.conidx = 0xFF;
                s_pad.frames = s_pad.scan_matches = s_pad.scan_reports = 0;
                s_pad.notify_count = 0;
                s_pad.raw_log = false;
                s_pad.scan_diag = false;
                s_pad.stick_log = false;
                s_pad.last_mask = 0;
                s_pad.last_stick_dir = 0;
                s_pad.last_stick_btn = 0;
                s_pad.l1_down = false;
                s_pad.l2_down = false;
                memset(&s_pad.peer, 0, sizeof(s_pad.peer));
                s_pad.peer_str[0] = 0;
                memset(s_peers, 0, sizeof(s_peers));
                int ret = lisa_ble_client_init(&s_pad_cb, NULL);
                if (ret != 0 && ret != -EALREADY) {
                    /* 开机自启时 BLE 栈可能尚未使能（ipc 就绪后才 init）。
                     * 不放弃：稍后重试；仅首次失败打日志避免刷屏。 */
                    if (s_pad.init_retries == 0) {
                        LISA_LOGW(TAG, "client init ret=%d, retry later", ret);
                    }
                    s_pad.init_retries++;
                    s_pad.started = false;
                    s_pad.init_retry_at =
                        xTaskGetTickCount() + pdMS_TO_TICKS(CPAD_INIT_RETRY_MS);
                    break;
                }
                LISA_LOGI(TAG, "client init ret=%d (retries=%u)",
                          ret, s_pad.init_retries);
                s_pad.init_retries = 0;
                s_pad.init_retry_at = 0;
            }
            /* 已连接/订阅中时不要打断；其余情况（重新）开始扫描 */
            if (s_pad.state == PAD_IDLE || s_pad.state == PAD_SCANNING) {
                pad_scan_start();
            }
            break;

        case EV_CMD_SCAN:
            if (s_pad.started) pad_scan_start();
            break;

        case EV_CMD_CONNECT:
            if (s_pad.started && (s_pad.state == PAD_SCANNING ||
                                  s_pad.state == PAD_IDLE)) {
                pad_connect_peer();
            }
            break;

        case EV_CMD_STOP:
            /* 无论是否已 started，都清掉开机自启的待重试，避免刚 off 又自启 */
            s_pad.init_retry_at = 0;
            s_pad.init_retries = 0;
            if (!s_pad.started) break;
            if (s_pad.state != PAD_SCANNING && s_pad.conidx != 0xFF) {
                lisa_ble_disconnect(s_pad.conidx, 0x13);
            }
            lisa_ble_scan_stop(GAP_SCAN_ID_0);
            lisa_ble_client_deinit();
            s_pad.started = false;
            s_pad.state = PAD_IDLE;
            s_pad.conidx = 0xFF;
            s_pad.last_mask = 0;
            s_pad.last_stick_dir = 0;
            s_pad.last_stick_btn = 0;
            s_pad.l1_down = false;
            s_pad.l2_down = false;
            gamepad_input_on_disconnect();
            gamepad_input_ble_set_active(false);
            LISA_LOGI(TAG, "stopped");
            break;

        case EV_SCAN_REPORT:
            pad_handle_scan_report(&ev);
            break;

        case EV_SCAN_ACTIVITY:
            LISA_LOGI(TAG, "scan activity active=%u id=%u status=0x%04X",
                      ev.u.actv.active, ev.u.actv.id, ev.u.actv.status);
            break;

        case EV_CONN_ACTIVITY:
            LISA_LOGI(TAG, "conn activity active=%u id=%u status=0x%04X",
                      ev.u.actv.active, ev.u.actv.id, ev.u.actv.status);
            break;

        case EV_CONN_FAILED:
            LISA_LOGE(TAG, "connection failed status=0x%04X", ev.u.status);
            if (s_pad.started) {
                s_pad.conidx = 0xFF;
                pad_scan_start();   /* 重新扫描等下一次 */
            }
            break;

        case EV_CONN_PARAMS:
            s_pad.conidx = ev.u.params.conidx;
            LISA_LOGI(TAG, "conn params conidx=%u intv=%u (%.2fms) lat=%u (%.1fms) timeout=%u (%ums)",
                      ev.u.params.conidx, ev.u.params.interval,
                      ev.u.params.interval * 1.25,
                      ev.u.params.latency, ev.u.params.latency * 1.25,
                      ev.u.params.timeout, ev.u.params.timeout * 10U);
            break;

        case EV_CONNECTED:
            /* 只接受我们自己发起的那次连接（state 停在 PAD_CONNECTING）。
             * 手机等外部设备连上本机（BLE 配网）时 state 是 SCANNING/其它，
             * 直接忽略，避免去对方身上做 0xFFA1 发现并误断开。 */
            if (s_pad.state != PAD_CONNECTING) {
                LISA_LOGI(TAG, "connected conidx=%u ignored (state %d, not ours)",
                          ev.u.connected.conidx, s_pad.state);
                break;
            }
            s_pad.conidx = ev.u.connected.conidx;
            LISA_LOGI(TAG, "connected conidx=%u", s_pad.conidx);
            pad_discover_start(s_pad.conidx);
            break;

        case EV_SERVICE:
            /* val/cccd 总是成对更新，避免跨服务错配 */
            if (ev.u.service.val_hdl) {
                s_pad.val_hdl = ev.u.service.val_hdl;
                s_pad.cccd_hdl = ev.u.service.cccd_hdl;
            }
            break;

        case EV_DISCOVER_COMPLETE:
            LISA_LOGI(TAG, "discover complete status=0x%04X val=0x%04X cccd=0x%04X",
                      ev.u.procedure.status, s_pad.val_hdl, s_pad.cccd_hdl);
            if (s_pad.state != PAD_DISCOVERING) break;
            if (ev.u.procedure.status != 0 || s_pad.val_hdl == 0 || s_pad.cccd_hdl == 0) {
                LISA_LOGE(TAG, "discover incomplete (0xFFA1/CCCD not found)");
                lisa_ble_disconnect(s_pad.conidx, 0x13);
                break;
            }
            pad_subscribe(s_pad.conidx);
            break;

        case EV_WRITE_COMPLETE:
            LISA_LOGI(TAG, "write complete hdl=0x%04X status=0x%04X",
                      ev.u.write.hdl, ev.u.write.status);
            if (s_pad.state == PAD_SUBSCRIBING && ev.u.write.hdl == s_pad.cccd_hdl &&
                ev.u.write.status == 0) {
                s_pad.state = PAD_RUNNING;
                /* BLE 手柄接管道: 网络手柄 (WS/UDP) 输入让位, 否则它的
                 * reset/静默松键会持续清掉这里按住的键 */
                gamepad_input_ble_set_active(true);
                LISA_LOGI(TAG, "subscribed, pad RUNNING");
            }
            break;

        case EV_NOTIFICATION: {
            const uint8_t *d = ev.u.notify.data;
            s_pad.notify_count++;
            if (s_pad.raw_log) {
                LISA_LOGI(TAG, "notify: hdl=0x%04X len=%u (want hdl=0x%04X len=%u)",
                          ev.u.notify.hdl, ev.u.notify.len,
                          s_pad.val_hdl, CPAD_FRAME_LEN);
            }
            if (s_pad.state == PAD_RUNNING && ev.u.notify.len == CPAD_FRAME_LEN &&
                ev.u.notify.hdl == s_pad.val_hdl) {
                uint32_t buttons;
                uint16_t dir, mask;
                memcpy(&buttons, d, 4);
                dir = cpad_stick_to_dpad(d[4], d[5]);
                mask = (uint16_t)(cpad_buttons_to_mask(buttons) | dir);
                gamepad_input_set_mask(mask);
                s_pad.last_mask = mask;
                s_pad.keepalive_at = xTaskGetTickCount() + pdMS_TO_TICKS(CPAD_KEEPALIVE_MS);
                s_pad.frames++;
                s_pad.dbg_raw = buttons;
                s_pad.dbg_buttons = (uint8_t)buttons;
                s_pad.dbg_lx = d[4];
                s_pad.dbg_ly = d[5];

                /* L1/L2 上升沿 → 进入/退出语音模式（按住不重复触发）。
                 * 两键都不参与线上位图，只做语音交互、不影响游戏按键 */
                if ((buttons & CPAD_L1) && !s_pad.l1_down) {
                    pad_trigger_wakeup();
                }
                s_pad.l1_down = (buttons & CPAD_L1) != 0;

                if ((buttons & CPAD_L2) && !s_pad.l2_down) {
                    pad_trigger_voice_exit();
                }
                s_pad.l2_down = (buttons & CPAD_L2) != 0;

                /* 摇杆事件：方向或摇杆按键（L3/R3）变化才打印，避免刷屏 */
                if (s_pad.stick_log &&
                    (dir != s_pad.last_stick_dir ||
                     (buttons & (CPAD_L3 | CPAD_R3)) != s_pad.last_stick_btn)) {
                    char ds[24];
                    s_pad.last_stick_dir = dir;
                    s_pad.last_stick_btn = buttons & (CPAD_L3 | CPAD_R3);
                    pad_dir_str(dir, ds, sizeof(ds));
                    LISA_LOGI(TAG, "stick: dir=%-12s LX=%3u LY=%3u "
                              "L3=%u R3=%u -> mask=0x%04X",
                              ds, d[4], d[5],
                              (unsigned)((buttons & CPAD_L3) ? 1 : 0),
                              (unsigned)((buttons & CPAD_R3) ? 1 : 0), mask);
                }
                if (s_pad.raw_log) {
                    char ds[24];
                    pad_dir_str(dir, ds, sizeof(ds));
                    LISA_LOGI(TAG, "raw frame: btn=0x%05X LX=%u LY=%u RX=%u RY=%u "
                              "-> mask=0x%04X (dpad=0x%04X stick=%s)",
                              buttons, d[4], d[5], d[6], d[7], mask,
                              (unsigned)cpad_buttons_to_mask(buttons), ds);
                }
            }
            break;
        }

        case EV_SET_RAW_LOG:
            s_pad.raw_log = (ev.u.status != 0);
            LISA_LOGI(TAG, "raw log %s", s_pad.raw_log ? "on" : "off");
            break;

        case EV_SET_SCAN_DIAG:
            s_pad.scan_diag = (ev.u.status != 0);
            LISA_LOGI(TAG, "scan diag %s", s_pad.scan_diag ? "on" : "off");
            break;

        case EV_SET_STICK_LOG:
            s_pad.stick_log = (ev.u.status != 0);
            LISA_LOGI(TAG, "stick log %s", s_pad.stick_log ? "on" : "off");
            break;

        case EV_DISCONNECTED:
            LISA_LOGI(TAG, "disconnected conidx=%u reason=0x%04X",
                      ev.u.disconnected.conidx, ev.u.disconnected.reason);
            gamepad_input_on_disconnect();
            gamepad_input_ble_set_active(false);
            s_pad.val_hdl = s_pad.cccd_hdl = 0;
            s_pad.last_mask = 0;
            s_pad.last_stick_dir = 0;
            s_pad.last_stick_btn = 0;
            s_pad.l1_down = false;
            s_pad.l2_down = false;
            if (s_pad.started) {
                s_pad.conidx = 0xFF;
                pad_scan_start();
            } else {
                s_pad.state = PAD_IDLE;
            }
            break;

        default:
            break;
        }

watchdog:
        /* 按住保活：手柄是事件驱动上报，按住不放期间不会有新帧；周期性
         * 把最近一次位图重写回 gamepad 层，保证小应用侧持续看到「按住」，
         * 同时覆盖输入层被其它手柄通道（WS 连接时 reset / UDP 静默松键）
         * 意外清零的情况。位图未变时重写不会产生新的短按锁存。 */
        if (s_pad.state == PAD_RUNNING && s_pad.last_mask != 0 &&
            (int32_t)(xTaskGetTickCount() - s_pad.keepalive_at) >= 0) {
            gamepad_input_set_mask(s_pad.last_mask);
            s_pad.keepalive_at = xTaskGetTickCount() + pdMS_TO_TICKS(CPAD_KEEPALIVE_MS);
        }

        /* 开机自启但 BLE 栈尚未就绪：到点重投启动命令重试 client init */
        if (!s_pad.started && s_pad.init_retry_at != 0 &&
            (int32_t)(xTaskGetTickCount() - s_pad.init_retry_at) >= 0) {
            s_pad.init_retry_at = 0;
            pad_send_cmd(EV_CMD_START);
        }

        /* 连接/发现/订阅阶段超时未推进 → 断开退回扫描，避免挂死 */
        if (s_pad.started &&
            (s_pad.state == PAD_CONNECTING || s_pad.state == PAD_DISCOVERING ||
             s_pad.state == PAD_SUBSCRIBING) &&
            (int32_t)(xTaskGetTickCount() - s_pad.connect_deadline) >= 0) {
            LISA_LOGW(TAG, "state %d timeout, abort and rescan", s_pad.state);
            if (s_pad.state != PAD_CONNECTING && s_pad.conidx != 0xFF) {
                lisa_ble_disconnect(s_pad.conidx, 0x13);
            }
            s_pad.conidx = 0xFF;
            s_pad.val_hdl = s_pad.cccd_hdl = 0;
            s_pad.last_mask = 0;
            s_pad.last_stick_dir = 0;
            s_pad.last_stick_btn = 0;
            s_pad.l1_down = false;
            s_pad.l2_down = false;
            gamepad_input_on_disconnect();
            gamepad_input_ble_set_active(false);
            pad_scan_start();
        }
    }
}

/* ================================================================== */
/* 公开 API（任意线程）                                                 */
/* ================================================================== */

static void pad_send_cmd(uint8_t cmd) {
    pad_evt_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = cmd;
    pad_post(&ev);
}

static bool pad_ensure_task(void) {
    if (!s_evq) {
        s_evq = xQueueCreate(PAD_EVQ_LEN, sizeof(pad_evt_t));
        if (!s_evq) return false;
    }
    if (!s_task) {
        lisa_thread_attr_t attr;
        attr.name = (uint8_t *)"ble_pad";
        attr.stack_size = 4 * 1024;
        attr.priority = LISA_OS_PRIORITY_ABOVE_NORMAL;
        s_task = lisa_thread_create(&attr, pad_task, NULL);
        if (!s_task) return false;
    }
    return true;
}

int ble_pad_start(void) {
    if (!pad_ensure_task()) return -1;
    pad_send_cmd(EV_CMD_START);   /* 幂等：已启动则仅按需重开扫描 */
    return 0;
}

int ble_pad_stop(void) {
    if (!s_pad.started) return 0;
    pad_send_cmd(EV_CMD_STOP);
    return 0;
}

bool ble_pad_is_running(void) {
    return s_pad.started;
}

void ble_pad_get_status(ble_pad_status_t *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->running = s_pad.started;
    out->connected = (s_pad.state == PAD_RUNNING);
    memcpy(out->peer_addr, s_pad.peer_str, sizeof(out->peer_addr));
    out->last_rssi = s_pad.last_rssi;
    out->frames = s_pad.frames;
    out->scan_matches = s_pad.scan_matches;
    out->conidx = s_pad.conidx;
    out->last_buttons = s_pad.dbg_buttons;
    out->last_lx = s_pad.dbg_lx;
    out->last_ly = s_pad.dbg_ly;
}

void ble_pad_shell_cmd(const char *sub) {
    if (!sub) return;
    if (strcmp(sub, "scan") == 0) {
        if (s_pad.started) pad_send_cmd(EV_CMD_SCAN);   /* 已启动：只重开扫描 */
        else ble_pad_start();                            /* 未启动：init + 扫描 */
    }
    else if (strcmp(sub, "on") == 0) { ble_pad_start(); }
    else if (strcmp(sub, "conn") == 0 || strcmp(sub, "connect") == 0) {
        pad_send_cmd(EV_CMD_CONNECT);
    }
    else if (strcmp(sub, "off") == 0) { ble_pad_stop(); }
    else if (strcmp(sub, "raw") == 0) {
        pad_evt_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = EV_SET_RAW_LOG;
        ev.u.status = s_pad.raw_log ? 0 : 1;   /* toggle */
        pad_post(&ev);
    }
    else if (strcmp(sub, "diag") == 0) {
        pad_evt_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = EV_SET_SCAN_DIAG;
        ev.u.status = s_pad.scan_diag ? 0 : 1;   /* toggle */
        pad_post(&ev);
    }
    else if (strcmp(sub, "stick") == 0) {
        pad_evt_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = EV_SET_STICK_LOG;
        ev.u.status = s_pad.stick_log ? 0 : 1;   /* toggle */
        pad_post(&ev);
    }
}

/* ================================================================== */
/* shell: blepad scan|conn|on|off|raw|status                           */
/* ================================================================== */

static int blepad_shell_cmd(int argc, char **argv) {
    if (argc < 2) {
        shellPrint(shellGetCurrent(),
                   "usage: blepad scan|conn|on|off|raw|stick|diag|status\n"
                   "  on     扫描→识别 CodexPad→连接→GATT→订阅（全自动）\n"
                   "  scan   开始扫描    conn   手动连接最近发现的 CodexPad\n"
                   "  raw    切换原始输入帧打印（含按键+摇杆，核对键位）\n"
                   "  stick  切换摇杆事件打印（方向 / L3 R3，变化才打）\n"
                   "  diag   切换广播流量总览打印（默认关，排查收不到广播用）\n"
                   "  off    停止        status 状态\n");
        return 0;
    }
    if (strcmp(argv[1], "status") == 0) {
        ble_pad_status_t st;
        char ds[24];
        ble_pad_get_status(&st);
        pad_dir_str(s_pad.last_stick_dir, ds, sizeof(ds));
        shellPrint(shellGetCurrent(),
                   "blepad: state=%d running=%d connected=%d peer=%s raw=%d stick=%d diag=%d\n"
                   "        reports=%u matches=%u frames=%u notifies=%u rssi=%d\n"
                   "        raw_btn=0x%05X mask=0x%04X LX=%u LY=%u stick_dir=%s\n",
                   s_pad.state, st.running, st.connected, st.peer_addr,
                   (int)s_pad.raw_log, (int)s_pad.stick_log, (int)s_pad.scan_diag,
                   s_pad.scan_reports, st.scan_matches, st.frames,
                   s_pad.notify_count, st.last_rssi,
                   s_pad.dbg_raw, s_pad.last_mask,
                   st.last_lx, st.last_ly, ds);
        return 0;
    }
    ble_pad_shell_cmd(argv[1]);
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 blepad, blepad_shell_cmd, CodexPad BLE gamepad driver);
