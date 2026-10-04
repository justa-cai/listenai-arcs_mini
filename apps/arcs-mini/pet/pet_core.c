/*
 * pet_core.c - virtual pet state machine (pure logic)
 *
 * All stat math uses x10 fixed point (0..1000 == 0..100 display points).
 * Decay is applied in elapsed-time chunks against an {wall, uptime} anchor;
 * while the wall clock is not valid, uptime deltas are used, so a powered-off
 * device without a valid clock simply freezes the pet. Powered-off time with
 * a valid clock on both sides is caught up at boot with a 0.6 decay discount
 * (capped at 48 h).
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "listen_system.h"
#include "semphr.h"

#include "lisa_time.h"
#include "pet_core.h"
#include "pet_save.h"
#include "voice_msg.h"

#define TAG "pet"

/* ---------------- tuning (x10 points per hour) ---------------- */
#define RATE_SATIETY_AWAKE 24  /* 2.4/h */
#define RATE_SATIETY_SLEEP 8
#define RATE_HAPPY_AWAKE 26
#define RATE_HAPPY_SLEEP 3
#define RATE_HAPPY_PER_POOP 5
#define RATE_CLEAN_BASE 4
#define RATE_CLEAN_PER_POOP 15
#define RATE_ENERGY_AWAKE 35 /* decay 3.5/h */
#define RATE_ENERGY_SLEEP 130 /* recover 13/h */
#define SICK_MULT_X10 14 /* decay x1.4 while sick */
#define OFFLINE_CAP_S (48u * 3600u)
#define OFFLINE_DISCOUNT_X10 6
#define HATCH_CARE_COUNT 3
#define HATCH_FALLBACK_S 3600u
#define SICK_AFTER_ZERO_S (6u * 3600u)
#define REMIND_COOLDOWN_S 1800u
#define REMIND_MISS_S 86400u
#define POOP_MIN_POOPS 3

#define SAT_FULL 900
#define SAT_REJECT 900
#define CLEAN_SPOTLESS 950
#define ENERGY_TIRED 150
#define ENERGY_RESTED 950
#define NIGHT_START_H 22
#define NIGHT_END_H 7

static pet_persist_t st;
static SemaphoreHandle_t s_lock;
static uint32_t s_pending_evt;
static uint32_t s_remind_cd[6]; /* hungry/sad/dirty/tired/sick/miss */
static bool s_below_latch[4];   /* reminder hysteresis per stat */
static uint32_t s_save_throttle;
static uint32_t s_rand_seed = 0x9E3779B9u;

/* Fractional decay accumulator per stat (x10-points * ms). Rates are only
 * a few x10-points per HOUR, so a 5 s tick would truncate to zero without
 * carrying the remainder forward. 1 x10-point == 3_600_000 stat_ms units. */
static uint64_t s_stat_acc[4];

static uint32_t pet_rand(void)
{
    s_rand_seed ^= s_rand_seed << 13;
    s_rand_seed ^= s_rand_seed >> 17;
    s_rand_seed ^= s_rand_seed << 5;
    return s_rand_seed;
}

static void pet_core_probe_cb(void *unused, uint32_t msg_id, void *data, uint32_t len,
                              void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;
    pet_core_on_clock_valid();
}

static void evt_raise(uint32_t bits)
{
    s_pending_evt |= bits;
}

/* ---------------- time ---------------- */

static void now_pair(bool *wall_valid, uint64_t *wall_s, uint64_t *uptime_ms)
{
    struct timeval tv;
    *uptime_ms = lisa_os_get_tick_ms();
    if (ls_sys_time_is_valid() && ls_sys_get_time(&tv) == 0 && tv.tv_sec > 1600000000) {
        *wall_valid = true;
        *wall_s = (uint64_t)tv.tv_sec;
    } else {
        *wall_valid = false;
        *wall_s = 0;
    }
}

/* Elapsed ms since the anchor; 0 when the time domain changed (freeze). */
static uint64_t elapsed_ms(void)
{
    bool wv;
    uint64_t wall, up;
    now_pair(&wv, &wall, &up);

    if (st.anchor_wall_valid) {
        if (!wv) {
            return 0; /* clock lost: freeze until it comes back */
        }
        if (wall < st.anchor_wall_s) {
            return 0;
        }
        uint64_t dt = (wall - st.anchor_wall_s) * 1000u;
        return dt > 0xFFFFFFFFu ? 0xFFFFFFFFu : dt;
    }
    if (wv) {
        return 0; /* domain switch handled by re-anchor in on_clock_valid */
    }
    if (up < st.anchor_uptime_ms) {
        return 0;
    }
    uint64_t dt = up - st.anchor_uptime_ms;
    return dt > 0xFFFFFFFFu ? 0xFFFFFFFFu : dt;
}

static void re_anchor(void)
{
    bool wv;
    uint64_t wall, up;
    now_pair(&wv, &wall, &up);
    st.anchor_wall_valid = wv ? 1 : 0;
    st.anchor_wall_s = wall;
    st.anchor_uptime_ms = up;
}

static bool is_night_wall(void)
{
    struct tm cal;
    if (!ls_sys_time_is_valid() || ls_sys_get_localtime(&cal) != 0) {
        return false;
    }
    return cal.tm_hour >= NIGHT_START_H || cal.tm_hour < NIGHT_END_H;
}

/* ---------------- state transitions ---------------- */

static void stat_sub(uint16_t *v, uint32_t amount)
{
    *v = (*v > amount) ? (uint16_t)(*v - amount) : 0;
}

static void stat_add(uint16_t *v, uint32_t amount)
{
    uint32_t sum = *v + amount;
    *v = (sum > 1000) ? 1000 : (uint16_t)sum;
}

static pet_mood_t derive_mood(void)
{
    if (st.stage == PET_STAGE_EGG) {
        return st.egg_age_s >= 2400 ? PET_MOOD_EGG_WOBBLE : PET_MOOD_EGG_CALM;
    }
    if (st.sleeping) {
        return PET_MOOD_SLEEP;
    }
    if (st.sick) {
        return PET_MOOD_SICK;
    }
    uint16_t m[4] = {st.satiety, st.clean, st.happy, st.energy};
    /* priority order: satiety > clean > happy > energy on ties */
    int worst = 0;
    for (int i = 1; i < 4; i++) {
        if (m[i] < m[worst]) {
            worst = i;
        }
    }
    if (m[worst] < 300) {
        switch (worst) {
        case 0: return PET_MOOD_HUNGRY;
        case 1: return PET_MOOD_DIRTY;
        case 2: return PET_MOOD_SAD;
        default: return PET_MOOD_TIRED;
        }
    }
    uint32_t avg = ((uint32_t)st.satiety + st.happy + st.clean + st.energy) / 4;
    if (avg >= 750 && m[worst] >= 600) {
        return PET_MOOD_CONTENT;
    }
    return PET_MOOD_NORMAL;
}

static void day_cut(void)
{
    st.age_days++;
    if (st.good_hours >= 12 && st.good_days < 0xFFFF) {
        st.good_days++;
    }
    st.good_hours = 0;
}

static void check_reminders(void)
{
    struct {
        uint16_t val;
        bool *latch;
        uint32_t *cd;
        uint32_t evt;
    } r[4] = {
        {st.satiety, &s_below_latch[0], &s_remind_cd[0], PET_REMIND_HUNGRY},
        {st.happy, &s_below_latch[1], &s_remind_cd[1], PET_REMIND_SAD},
        {st.clean, &s_below_latch[2], &s_remind_cd[2], PET_REMIND_DIRTY},
        {st.energy, &s_below_latch[3], &s_remind_cd[3], PET_REMIND_TIRED},
    };
    for (int i = 0; i < 4; i++) {
        if (r[i].val < 300) {
            if (!*r[i].latch && *r[i].cd == 0) {
                evt_raise(r[i].evt);
                *r[i].cd = REMIND_COOLDOWN_S;
                *r[i].latch = true;
            }
        } else if (r[i].val >= 400) {
            *r[i].latch = false;
        }
    }
    if (st.sick && s_remind_cd[4] == 0) {
        evt_raise(PET_REMIND_SICK);
        s_remind_cd[4] = REMIND_COOLDOWN_S;
    }
    if (st.care_gap_s >= REMIND_MISS_S && s_remind_cd[5] == 0) {
        evt_raise(PET_REMIND_MISS);
        s_remind_cd[5] = 12u * 3600u;
    }
}

static void check_evolution(void)
{
    if (st.stage == PET_STAGE_EGG) {
        if (st.care_count >= HATCH_CARE_COUNT || st.egg_age_s >= HATCH_FALLBACK_S) {
            st.stage = PET_STAGE_BABY;
            bool wv;
            uint64_t wall, up;
            now_pair(&wv, &wall, &up);
            st.born_wall_s = wv ? (int64_t)wall : -1;
            evt_raise(PET_EVT_HATCH);
        }
    } else if (st.stage == PET_STAGE_BABY) {
        if ((st.good_days >= 3 && st.age_days >= 4) || st.age_days >= 7) {
            st.stage = PET_STAGE_ADULT;
            evt_raise(PET_EVT_EVOLVE);
        }
    }
}

/* Advance one stat by rate (x10-points/hour) over dt_ms, carrying the
 * sub-point remainder. sign < 0 decays, sign > 0 recovers. */
static void stat_advance(int idx, uint16_t *v, uint32_t rate_x10_per_hour, int sign,
                         uint64_t dt_ms)
{
    if (rate_x10_per_hour == 0) {
        return;
    }
    s_stat_acc[idx] += (uint64_t)rate_x10_per_hour * dt_ms;
    uint32_t delta = (uint32_t)(s_stat_acc[idx] / 3600000u);
    s_stat_acc[idx] %= 3600000u;
    if (delta == 0) {
        return;
    }
    if (sign < 0) {
        stat_sub(v, delta);
    } else {
        stat_add(v, delta);
    }
}

/* Advance everything by dt_ms. discount_x10 scales stat changes only. */
static void apply_elapsed(uint64_t dt_ms, int discount_x10)
{
    uint64_t dt_s64 = dt_ms / 1000u;
    if (dt_s64 == 0) {
        return;
    }
    if (dt_s64 > OFFLINE_CAP_S) {
        dt_s64 = OFFLINE_CAP_S;
    }
    uint32_t dt_s = (uint32_t)dt_s64;

    int mult = st.sick ? SICK_MULT_X10 : 10;
    int dm = discount_x10;

    /* per-stat rates in x10-points/hour (poop penalties always apply) */
    uint32_t poop_happy = (uint32_t)st.poops * RATE_HAPPY_PER_POOP;
    uint32_t poop_clean = (uint32_t)st.poops * RATE_CLEAN_PER_POOP;
    uint16_t *stats_x10[4] = {&st.satiety, &st.happy, &st.clean, &st.energy};
    uint32_t decay_rate[4];
    uint32_t recover_rate[4] = {0, 0, 0, 0};

    if (!st.sleeping) {
        decay_rate[0] = RATE_SATIETY_AWAKE;
        decay_rate[1] = RATE_HAPPY_AWAKE + poop_happy;
        decay_rate[2] = RATE_CLEAN_BASE + poop_clean;
        decay_rate[3] = RATE_ENERGY_AWAKE;
    } else {
        decay_rate[0] = RATE_SATIETY_SLEEP;
        decay_rate[1] = RATE_HAPPY_SLEEP + poop_happy;
        decay_rate[2] = RATE_CLEAN_BASE + poop_clean;
        decay_rate[3] = 0;
        recover_rate[3] = RATE_ENERGY_SLEEP;
    }

    for (int i = 0; i < 4; i++) {
        if (decay_rate[i]) {
            stat_advance(i, stats_x10[i], decay_rate[i] * mult / 10 * dm / 10, -1, dt_ms);
        }
        if (recover_rate[i]) {
            stat_advance(i, stats_x10[i], recover_rate[i] * dm / 10, +1, dt_ms);
        }
    }

    /* time accumulators advance at full speed */
    if (st.stage == PET_STAGE_EGG) {
        st.egg_age_s += dt_s;
    }
    st.day_elapsed_s += dt_s;
    while (st.day_elapsed_s >= 86400u) {
        st.day_elapsed_s -= 86400u;
        day_cut();
    }
    st.hour_elapsed_s += dt_s;
    if (st.hour_elapsed_s >= 3600u) {
        st.hour_elapsed_s -= 3600u;
        uint32_t avg = ((uint32_t)st.satiety + st.happy + st.clean + st.energy) / 4;
        if (avg >= 550 && st.good_hours < 16) {
            st.good_hours++;
        }
    }
    st.care_gap_s += dt_s;

    /* sickness accumulation */
    if (st.satiety == 0 || st.happy == 0 || st.clean == 0 || st.energy == 0) {
        if (st.zero_duration_s < 0x7FFFFFF0) {
            st.zero_duration_s += dt_s;
        }
        if (!st.sick && st.zero_duration_s >= SICK_AFTER_ZERO_S) {
            st.sick = 1;
            evt_raise(PET_REMIND_SICK);
        }
    } else {
        st.zero_duration_s = 0;
    }

    /* poop countdown */
    if (st.poop_due_s > 0) {
        if ((uint32_t)st.poop_due_s > dt_s) {
            st.poop_due_s -= (int32_t)dt_s;
        } else if (st.poops < POOP_MIN_POOPS) {
            st.poops++;
            st.poop_due_s = -1;
        } else {
            st.poop_due_s = -1;
        }
    }

    /* reminder cooldowns */
    for (int i = 0; i < 6; i++) {
        if (s_remind_cd[i] > dt_s) {
            s_remind_cd[i] -= dt_s;
        } else {
            s_remind_cd[i] = 0;
        }
    }

    check_reminders();
    check_evolution();
}

static void autosleep_check(void)
{
    if (st.stage == PET_STAGE_EGG || !ls_sys_time_is_valid()) {
        return; /* never auto-sleep on a fake clock */
    }
    if (!st.sleeping && is_night_wall() && st.energy < ENERGY_RESTED) {
        st.sleeping = 1;
        evt_raise(PET_EVT_SLEPT);
    } else if (st.sleeping && !is_night_wall()) {
        st.sleeping = 0;
        evt_raise(PET_EVT_WOKE);
    }
}

/* ---------------- actions ---------------- */

static void result_set(pet_result_t *out, int code, const char *fmt, ...)
{
    va_list args;
    out->code = code;
    va_start(args, fmt);
    vsnprintf(out->tts, sizeof(out->tts), fmt, args);
    va_end(args);
}

static int is_care_action(uint8_t action)
{
    return action == PET_ACTION_FEED || action == PET_ACTION_CLEAN ||
           action == PET_ACTION_PLAY || action == PET_ACTION_MEDICINE;
}

static int pet_do_action(uint8_t action, uint8_t item, pet_result_t *out)
{
    /* bring state to now before mutating */
    uint64_t dt = elapsed_ms();
    if (dt >= 1000) {
        apply_elapsed(dt, 10);
    }
    re_anchor();

    if (action >= PET_ACTION_NUM) {
        result_set(out, -2, "我不太明白这个指令");
        return -2;
    }

    /* egg stage: gentle care still counts toward hatching */
    if (st.stage == PET_STAGE_EGG) {
        switch (action) {
        case PET_ACTION_FEED:
        case PET_ACTION_CLEAN:
        case PET_ACTION_PLAY:
        case PET_ACTION_MEDICINE:
            if (st.care_count < 0xFFFF) {
                st.care_count++;
            }
            st.care_gap_s = 0;
            check_evolution();
            evt_raise(action == PET_ACTION_FEED ? PET_EVT_EAT
                      : action == PET_ACTION_CLEAN ? PET_EVT_CLEAN
                      : action == PET_ACTION_PLAY   ? PET_EVT_PLAY
                                                    : PET_EVT_MEDICINE);
            if (st.stage != PET_STAGE_EGG) {
                result_set(out, 0, "咔嚓……哇！我出生啦！");
            } else {
                result_set(out, 0, "蛋好像轻轻晃了一下，再照顾 %d 次就能孵化啦",
                           HATCH_CARE_COUNT - st.care_count);
            }
            goto save_out;
        case PET_ACTION_STATUS:
            result_set(out, 0, "蛋静静地躺着，好像有小生命在里面，再照顾 %d 次就能孵化啦",
                       HATCH_CARE_COUNT - st.care_count);
            return 0;
        case PET_ACTION_LIGHT:
        case PET_ACTION_NEXT:
        case PET_ACTION_CONFIRM:
        case PET_ACTION_CANCEL:
            result_set(out, -1, "蛋还睡不醒，先等等它孵化吧");
            return -1;
        default:
            break;
        }
    }

    switch (action) {
    case PET_ACTION_FEED:
        if (st.sleeping) {
            result_set(out, -1, "zzZ……先说开灯叫醒它吧");
            return -1;
        }
        if (st.satiety >= SAT_REJECT) {
            evt_raise(PET_EVT_REJECT);
            result_set(out, -1, "我超级饱，一口都吃不下啦");
            return -1;
        }
        if (item == PET_ITEM_SNACK) {
            stat_add(&st.satiety, 180);
            stat_add(&st.happy, 80);
            evt_raise(PET_EVT_EAT);
            result_set(out, 0, "小零食最开心了！开心度到 %d 啦", st.happy / 10);
        } else {
            stat_add(&st.satiety, 450);
            stat_add(&st.happy, 20);
            if (st.poop_due_s < 0) {
                st.poop_due_s = (int32_t)(10800 + pet_rand() % 3600 - 1800);
            }
            evt_raise(PET_EVT_EAT);
            result_set(out, 0, "啊呜啊呜~真好吃！饱食度到 %d 啦", st.satiety / 10);
        }
        break;

    case PET_ACTION_CLEAN:
        if (st.sleeping) {
            result_set(out, -1, "zzZ……先说开灯叫醒它吧");
            return -1;
        }
        if (st.clean >= CLEAN_SPOTLESS && st.poops == 0) {
            result_set(out, 0, "我本来就很干净呀");
            return 0;
        }
        st.clean = 1000;
        st.poops = 0;
        stat_add(&st.happy, 60);
        evt_raise(PET_EVT_CLEAN);
        result_set(out, 0, "洗得香香软软的~清洁度到 %d 啦", 100);
        break;

    case PET_ACTION_PLAY:
        if (st.sleeping) {
            result_set(out, -1, "zzZ……先说开灯叫醒它吧");
            return -1;
        }
        if (st.sick) {
            evt_raise(PET_EVT_REJECT);
            result_set(out, -1, "我生病了没力气玩，先给我吃点药吧");
            return -1;
        }
        if (st.energy < ENERGY_TIRED) {
            evt_raise(PET_EVT_REJECT);
            result_set(out, -1, "呼…跑不动了，让我先睡一觉吧（精力 %d）", st.energy / 10);
            return -1;
        }
        stat_add(&st.happy, 320);
        stat_sub(&st.energy, 120);
        stat_sub(&st.satiety, 40);
        evt_raise(PET_EVT_PLAY);
        result_set(out, 0, "耶！蹦蹦跳跳最开心了！开心度到 %d 啦", st.happy / 10);
        break;

    case PET_ACTION_LIGHT:
        if (item == PET_ITEM_LIGHT_WAKE) {
            if (!st.sleeping) {
                result_set(out, 0, "我已经醒着啦");
                return 0;
            }
            st.sleeping = 0;
            evt_raise(PET_EVT_WOKE);
            result_set(out, 0, "早上好！精力 %d", st.energy / 10);
            break;
        }
        if (item == PET_ITEM_LIGHT_SLEEP) {
            if (st.sleeping) {
                result_set(out, 0, "zzZ……我已经睡着啦");
                return 0;
            }
            st.sleeping = 1;
            evt_raise(PET_EVT_SLEPT);
            result_set(out, 0, "晚安，做个好梦~");
            break;
        }
        if (st.sleeping) {
            st.sleeping = 0;
            evt_raise(PET_EVT_WOKE);
            result_set(out, 0, "早上好！精力 %d", st.energy / 10);
            break;
        }
        if (st.energy >= ENERGY_RESTED && !is_night_wall()) {
            evt_raise(PET_EVT_REJECT);
            result_set(out, -1, "一点都不困，还想再玩会儿！");
            return -1;
        }
        st.sleeping = 1;
        evt_raise(PET_EVT_SLEPT);
        result_set(out, 0, "晚安，做个好梦~");
        break;

    case PET_ACTION_MEDICINE:
        if (!st.sick) {
            evt_raise(PET_EVT_REJECT);
            result_set(out, -1, "我又没生病，不要吃药啦");
            return -1;
        }
        st.sick = 0;
        st.zero_duration_s = 0;
        stat_add(&st.energy, 200);
        stat_add(&st.happy, 100);
        evt_raise(PET_EVT_MEDICINE);
        result_set(out, 0, "药苦苦的…可是感觉好多了！");
        break;

    case PET_ACTION_STATUS: {
        static const char *mood_word[] = {
            [PET_MOOD_EGG_CALM] = "安安静静的", [PET_MOOD_EGG_WOBBLE] = "在动来动去",
            [PET_MOOD_SLEEP] = "睡得香香的",   [PET_MOOD_SICK] = "生病了，需要吃药",
            [PET_MOOD_HUNGRY] = "肚子饿扁了",  [PET_MOOD_SAD] = "有点无聊",
            [PET_MOOD_DIRTY] = "脏乎乎的",     [PET_MOOD_TIRED] = "困困的",
            [PET_MOOD_CONTENT] = "开心得冒泡", [PET_MOOD_NORMAL] = "还好啦",
        };
        static const char *stage_name[] = {"蛋", "幼年", "成年"};
        result_set(out, 0, "现在是%s第 %u 天。饱食 %u、开心 %u、清洁 %u、精力 %u，%s",
                   stage_name[st.stage], st.age_days + 1, st.satiety / 10, st.happy / 10,
                   st.clean / 10, st.energy / 10, mood_word[derive_mood()]);
        return 0;
    }

    case PET_ACTION_NEXT:
        result_set(out, 0, "好的");
        return 0;
    case PET_ACTION_CONFIRM:
        result_set(out, 0, "好的");
        return 0;
    case PET_ACTION_CANCEL:
        result_set(out, 0, "好的，先不点啦");
        return 0;
    case PET_ACTION_GREET:
    case PET_ACTION_LOVE:
    case PET_ACTION_PAT:
        if (st.stage == PET_STAGE_EGG) {
            result_set(out, -1, "蛋轻轻晃了一下，好像在回应你");
            return -1;
        }
        if (st.sleeping) {
            result_set(out, -1, "zzZ……它睡得正香，先说开灯叫醒它吧");
            return -1;
        }
        if (action == PET_ACTION_GREET) {
            stat_add(&st.happy, 50);
            evt_raise(PET_EVT_GREET);
            result_set(out, 0, "你好呀！见到你真开心！");
        } else if (action == PET_ACTION_LOVE) {
            stat_add(&st.happy, 80);
            evt_raise(PET_EVT_LOVE);
            result_set(out, 0, "我也爱你！");
        } else {
            stat_add(&st.happy, 60);
            stat_add(&st.energy, 30);
            evt_raise(PET_EVT_PAT);
            result_set(out, 0, "好舒服呀~");
        }
        break;
    default:
        result_set(out, -2, "我不太明白这个指令");
        return -2;
    }

    if (is_care_action(action)) {
        if (st.care_count < 0xFFFF) {
            st.care_count++;
        }
        st.care_gap_s = 0;
        check_evolution();
    }

save_out:
    pet_save_write(&st);
    return out->code;
}

/* ---------------- public API ---------------- */

int pet_core_init(void)
{
    if (!s_lock) {
        s_lock = xSemaphoreCreateMutex();
        if (!s_lock) {
            return -1;
        }
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);

    if (!pet_save_read(&st)) {
        memset(&st, 0, sizeof(st));
        st.satiety = 800;
        st.happy = 800;
        st.clean = 1000;
        st.energy = 900;
        st.stage = PET_STAGE_EGG;
        st.poop_due_s = -1;
        st.born_wall_s = -1;
        LISA_LOGI(TAG, "core: new egg created");
    } else {
        LISA_LOGI(TAG, "core: restored stage=%u sat=%u hap=%u cln=%u ene=%u age=%ud", st.stage,
                  st.satiety / 10, st.happy / 10, st.clean / 10, st.energy / 10, st.age_days);
        /* offline catch-up: both anchors on the wall clock */
        uint64_t dt = elapsed_ms();
        if (st.anchor_wall_valid && dt > 5000) {
            apply_elapsed(dt, OFFLINE_DISCOUNT_X10);
            LISA_LOGI(TAG, "core: offline catch-up %.1f h", dt / 3600000.0);
        }
    }

    bool wv;
    uint64_t wall, up;
    now_pair(&wv, &wall, &up);
    if (wv && st.born_wall_s < 0) {
        st.born_wall_s = (int64_t)wall;
    }
    re_anchor();
    pet_save_write(&st);
    xSemaphoreGive(s_lock);

    if (voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, pet_core_probe_cb, NULL) != 0) {
        LISA_LOGW(TAG, "core: probe subscribe failed (clock sync re-anchor disabled)");
    }
    return 0;
}

void pet_core_tick(void)
{
    if (!s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    /* clock became SNTP-valid after the anchor was taken (e.g. the probe
     * callback raced ahead of settimeofday): switch the anchor to the wall
     * domain now, dropping the sub-second gap. */
    if (!st.anchor_wall_valid && ls_sys_time_is_valid()) {
        re_anchor();
    }
    uint64_t dt = elapsed_ms();
    if (dt >= 5000) {
        apply_elapsed(dt, 10);
        re_anchor();
        autosleep_check();
    }
    s_save_throttle++;
    if (s_save_throttle >= 60) {
        s_save_throttle = 0;
        pet_save_write(&st);
    }
    xSemaphoreGive(s_lock);
}

int pet_core_action(uint8_t action, uint8_t item, pet_result_t *out)
{
    pet_result_t local;
    if (!out) {
        out = &local;
    }
    if (!s_lock) {
        result_set(out, -2, "宠物还在睡觉");
        return -2;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int rc = pet_do_action(action, item, out);
    xSemaphoreGive(s_lock);
    LISA_LOGI(TAG, "action %u/%u -> %d (%s)", action, item, out->code, out->tts);
    return rc;
}

void pet_core_snapshot(pet_snapshot_t *out)
{
    if (!out || !s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    out->satiety = st.satiety / 10;
    out->happy = st.happy / 10;
    out->clean = st.clean / 10;
    out->energy = st.energy / 10;
    out->stage = (pet_stage_t)st.stage;
    out->mood = derive_mood();
    out->sleeping = st.sleeping != 0;
    out->sick = st.sick != 0;
    out->wall_valid = ls_sys_time_is_valid();
    out->poops = st.poops;
    out->egg_crack = st.stage == PET_STAGE_EGG
                         ? (st.egg_age_s >= 2400 ? 2 : (st.egg_age_s >= 1200 ? 1 : 0))
                         : 0;
    out->age_days = st.age_days;
    out->care_count = st.care_count;
    out->evt_bits = s_pending_evt;
    xSemaphoreGive(s_lock);
}

void pet_core_evt_ack(uint32_t bits)
{
    if (!s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_pending_evt &= ~bits;
    xSemaphoreGive(s_lock);
}

void pet_core_on_clock_valid(void)
{
    if (!s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    /* PROBE_SUCCESS is published just BEFORE settimeofday: if the wall clock
     * is not valid yet, do nothing here; pet_core_tick() performs the domain
     * switch once it sees a valid clock. */
    if (!ls_sys_time_is_valid()) {
        xSemaphoreGive(s_lock);
        return;
    }
    /* apply any small pending delta, then switch the anchor to the wall
     * clock; from here on elapsed_ms() uses wall time */
    uint64_t dt = elapsed_ms();
    if (dt >= 5000) {
        apply_elapsed(dt, 10);
    }
    if (st.born_wall_s < 0) {
        bool wv;
        uint64_t wall, up;
        now_pair(&wv, &wall, &up);
        if (wv) {
            st.born_wall_s = (int64_t)wall;
        }
    }
    re_anchor();
    pet_save_write(&st);
    LISA_LOGI(TAG, "core: wall clock valid, re-anchored");
    xSemaphoreGive(s_lock);
}

void pet_core_flush_save(void)
{
    if (!s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint64_t dt = elapsed_ms();
    if (dt >= 5000) {
        apply_elapsed(dt, 10);
        re_anchor();
    }
    pet_save_write(&st);
    xSemaphoreGive(s_lock);
}

/* ---------------- debug/testing helpers ---------------- */

int pet_core_debug_set_stat(int idx, int val)
{
    if (!s_lock || idx < 0 || idx > 3 || val < 0 || val > 100) {
        return -1;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint64_t dt = elapsed_ms();
    if (dt >= 1000) {
        apply_elapsed(dt, 10);
    }
    uint16_t *stats[4] = {&st.satiety, &st.happy, &st.clean, &st.energy};
    *stats[idx] = (uint16_t)(val * 10);
    re_anchor();
    pet_save_write(&st);
    xSemaphoreGive(s_lock);
    return 0;
}

int pet_core_debug_set_stage(int stage)
{
    if (!s_lock || stage < PET_STAGE_EGG || stage > PET_STAGE_ADULT) {
        return -1;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    st.stage = (uint8_t)stage;
    pet_save_write(&st);
    xSemaphoreGive(s_lock);
    return 0;
}

int pet_core_debug_set_field(int field, int val)
{
    if (!s_lock) {
        return -1;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    switch (field) {
    case PET_DBG_SLEEPING:
        st.sleeping = val ? 1 : 0;
        break;
    case PET_DBG_SICK:
        st.sick = val ? 1 : 0;
        if (!st.sick) {
            st.zero_duration_s = 0;
        }
        break;
    case PET_DBG_POOPS:
        st.poops = (uint8_t)(val < 0 ? 0 : (val > 3 ? 3 : val));
        break;
    case PET_DBG_EGG_AGE_S:
        st.egg_age_s = (uint32_t)(val < 0 ? 0 : val);
        break;
    case PET_DBG_CARE_COUNT:
        st.care_count = (uint16_t)(val < 0 ? 0 : val);
        break;
    case PET_DBG_AGE_DAYS:
        st.age_days = (uint16_t)(val < 0 ? 0 : val);
        break;
    case PET_DBG_GOOD_DAYS:
        st.good_days = (uint16_t)(val < 0 ? 0 : val);
        break;
    default:
        xSemaphoreGive(s_lock);
        return -1;
    }
    pet_save_write(&st);
    xSemaphoreGive(s_lock);
    return 0;
}

int pet_core_debug_time_travel(uint32_t seconds)
{
    if (!s_lock || seconds == 0 || seconds > OFFLINE_CAP_S) {
        return -1;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint64_t dt = elapsed_ms();
    if (dt >= 1000) {
        apply_elapsed(dt, 10);
    }
    apply_elapsed((uint64_t)seconds * 1000u, 10);
    re_anchor(); /* the real clock did not move; drop the tiny real gap */
    pet_save_write(&st);
    LISA_LOGI(TAG, "dbg: time-travel %u s", seconds);
    xSemaphoreGive(s_lock);
    return 0;
}

int pet_core_debug_evt(uint32_t bits)
{
    if (!s_lock || !bits) {
        return -1;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_pending_evt |= bits;
    xSemaphoreGive(s_lock);
    return 0;
}

int pet_core_debug_reset(void)
{
    if (!s_lock) {
        return -1;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memset(&st, 0, sizeof(st));
    st.satiety = 800;
    st.happy = 800;
    st.clean = 1000;
    st.energy = 900;
    st.stage = PET_STAGE_EGG;
    st.poop_due_s = -1;
    st.born_wall_s = -1;
    memset(s_below_latch, 0, sizeof(s_below_latch));
    memset(s_remind_cd, 0, sizeof(s_remind_cd));
    s_pending_evt = 0;
    re_anchor();
    pet_save_write(&st);
    xSemaphoreGive(s_lock);
    LISA_LOGI(TAG, "dbg: reset to a fresh egg");
    return 0;
}
