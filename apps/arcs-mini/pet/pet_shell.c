/*
 * pet_shell.c - `pet` debug shell command
 *
 * Lets you drive the virtual pet without waiting for real time:
 *
 *   pet                          print state + usage
 *   pet set  <sat|hap|cln|ene> <0-100>      set a stat
 *   pet add  <sat|hap|cln|ene> <-100..100>  bump a stat
 *   pet stage <egg|baby|adult>              jump stage
 *   pet sleep <0|1>   pet sick <0|1>   pet poop <0-3>
 *   pet eggage <s>    pet care <n>    pet age <d>   pet good <d>
 *   pet time <seconds>              fast-forward the state machine
 *   pet act  <feed|snack|clean|play|light|wake|medicine|status|greet|love|pat>
 *   pet evt  <eat|clean|play|medicine|reject|hatch|evolve|slept|woke|greet|
 *             love|pat|rhungry|rsad|rdirty|rtired|rsick|rmiss>
 *   pet save | pet reset
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "extra/others/snapshot/lv_snapshot.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_ui_invoke.h"
#include "lisa_websocket.h"
#include "lvgl.h"
#include "pet_core.h"
#include "pet_ui.h"
#include "pet_voice.h"
#include "semphr.h"
#include "shell.h"

#define TAG "pet"

#define PET_SHOT_W 240
#define PET_SHOT_H 240
#define PET_SHOT_BYTES (PET_SHOT_W * PET_SHOT_H * 2) /* LVGL 16-bit screen */

static uint8_t *s_shot_buf;
static SemaphoreHandle_t s_shot_done;

/* runs on the UI thread: render the active screen offscreen and copy out */
static void pet_shot_worker(void *arg, uint32_t len)
{
    (void)arg;
    (void)len;
    lv_img_dsc_t *dsc = lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
    if (dsc && dsc->data && dsc->data_size >= PET_SHOT_BYTES && s_shot_buf) {
        memcpy(s_shot_buf, dsc->data, PET_SHOT_BYTES);
    } else {
        LISA_LOGE(TAG, "shot: snapshot failed (dsc=%p size=%u)", (void *)dsc,
                  dsc ? (unsigned)dsc->data_size : 0u);
        s_shot_buf[0] = 0xFF; /* mark failure */
    }
    if (dsc) {
        lv_snapshot_free(dsc);
    }
    xSemaphoreGive(s_shot_done);
}

static const char k_b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* ---------------- ws shot: push the frame to a host-side ws server ---------------- */

static SemaphoreHandle_t s_ws_conn; /* LISA_WS_ON_CONNECTED */
static SemaphoreHandle_t s_ws_done; /* server ack or disconnect */

static void pet_ws_on_event(lisa_ws_event_t *event)
{
    if (!event || !s_ws_done) {
        return;
    }
    if (event->what == LISA_WS_ON_CONNECTED) {
        if (s_ws_conn) {
            xSemaphoreGive(s_ws_conn);
        }
    } else {
        xSemaphoreGive(s_ws_done); /* disconnected/error: end the wait */
    }
}

static void pet_ws_on_data(lisa_ws_data_t *data)
{
    if (data && s_ws_done) {
        xSemaphoreGive(s_ws_done); /* server ack */
    }
}

/* url: ws://host:port/path  -> push raw RGB565 frame, wait for ack */
static int pet_cmd_shot_ws(const char *url)
{
    static char scheme[8], host[64], port[8], path[32];
    if (sscanf(url, "%7[^:]://%63[^:/]:%7[0-9]/%31s", scheme, host, port, path) < 3) {
        shellPrint(shellGetCurrent(), "pet: bad url, want ws://host:port/path\n");
        return -1;
    }

    /* capture first (same worker as the b64 path) */
    if (!s_shot_buf) {
        s_shot_buf = lisa_mem_alloc(PET_SHOT_BYTES);
        s_shot_done = xSemaphoreCreateBinary();
    }
    if (!s_shot_buf || !s_shot_done) {
        shellPrint(shellGetCurrent(), "pet: shot alloc failed\n");
        return -1;
    }
    (void)xSemaphoreTake(s_shot_done, 0);
    if (LISA_UI_INVOKE_UI(pet_shot_worker, NULL, 0) != 0 ||
        xSemaphoreTake(s_shot_done, pdMS_TO_TICKS(3000)) != pdTRUE) {
        shellPrint(shellGetCurrent(), "pet: shot capture failed\n");
        return -1;
    }

    if (!s_ws_done) {
        s_ws_done = xSemaphoreCreateBinary();
        s_ws_conn = xSemaphoreCreateBinary();
    }
    (void)xSemaphoreTake(s_ws_done, 0);
    (void)xSemaphoreTake(s_ws_conn, 0);

    uint8_t *scheme_u = (uint8_t *)scheme, *host_u = (uint8_t *)host;
    uint8_t *port_u = (uint8_t *)port, *path_u = (uint8_t *)path;
    lisa_ws_request_t req = {
        .scheme = scheme_u,
        .host = host_u,
        .port = port_u,
        .path = path_u,
        .timeout = 5000,
        .user = NULL,
        .on_event = pet_ws_on_event,
        .on_data = pet_ws_on_data,
    };
    lisa_ws_t *ws = lisa_ws_init(&req);
    if (!ws) {
        shellPrint(shellGetCurrent(), "pet: ws init failed\n");
        return -1;
    }
    if (lisa_ws_connect(ws) != LISA_WS_OK) {
        lisa_ws_cleanup(ws);
        shellPrint(shellGetCurrent(), "pet: ws connect failed\n");
        return -1;
    }
    /* connect() only starts the async handshake: wait for ON_CONNECTED */
    if (xSemaphoreTake(s_ws_conn, pdMS_TO_TICKS(8000)) != pdTRUE) {
        lisa_ws_cleanup(ws);
        shellPrint(shellGetCurrent(), "pet: ws handshake timeout\n");
        return -1;
    }

    char header[96];
    snprintf(header, sizeof(header), "{\"w\":%d,\"h\":%d,\"fmt\":\"rgb565\",\"len\":%u}",
             PET_SHOT_W, PET_SHOT_H, (unsigned)PET_SHOT_BYTES);
    int rc = -1;
    if (lisa_ws_send_text(ws, (const uint8_t *)header) == LISA_WS_OK &&
        lisa_ws_send_binary(ws, s_shot_buf, PET_SHOT_BYTES) == LISA_WS_OK) {
        /* wait for the server ack (or any event) */
        if (xSemaphoreTake(s_ws_done, pdMS_TO_TICKS(8000)) == pdTRUE) {
            rc = 0;
        }
    }
    lisa_ws_disconnect(ws);
    lisa_ws_cleanup(ws);

    shellPrint(shellGetCurrent(), "pet: ws shot %s (%u bytes)\n", rc == 0 ? "ok" : "FAILED",
               (unsigned)PET_SHOT_BYTES);
    return rc;
}

static int pet_cmd_shot(void)
{
    if (!s_shot_buf) {
        s_shot_buf = lisa_mem_alloc(PET_SHOT_BYTES);
        s_shot_done = xSemaphoreCreateBinary();
        if (!s_shot_buf || !s_shot_done) {
            shellPrint(shellGetCurrent(), "pet: shot alloc failed\n");
            return -1;
        }
    }
    (void)xSemaphoreTake(s_shot_done, 0);
    if (LISA_UI_INVOKE_UI(pet_shot_worker, NULL, 0) != 0) {
        shellPrint(shellGetCurrent(), "pet: shot invoke failed\n");
        return -1;
    }
    if (xSemaphoreTake(s_shot_done, pdMS_TO_TICKS(3000)) != pdTRUE) {
        shellPrint(shellGetCurrent(), "pet: shot timeout\n");
        return -1;
    }
    if (s_shot_buf[0] == 0xFF && s_shot_buf[1] == 0xFF && s_shot_buf[2] == 0xFF) {
        shellPrint(shellGetCurrent(), "pet: shot capture failed\n");
        return -1;
    }

    /* RLE-encode 16-bit runs in place (offset by the header kept in buf):
     * (u16 LE run length, u16 LE color); falls back to raw when RLE does
     * not help. Keeps the base64 payload a few KB instead of ~150 KB. */
    static uint8_t *rle;
    if (!rle) {
        rle = lisa_mem_alloc(PET_SHOT_BYTES + 4);
        if (!rle) {
            shellPrint(shellGetCurrent(), "pet: shot rle alloc failed\n");
            return -1;
        }
    }
    const uint16_t *px = (const uint16_t *)s_shot_buf;
    uint32_t total_px = PET_SHOT_BYTES / 2;
    uint32_t out = 0;
    for (uint32_t i = 0; i < total_px && out + 4 <= PET_SHOT_BYTES;) {
        uint16_t color = px[i];
        uint32_t run = 1;
        while (i + run < total_px && px[i + run] == color && run < 0xFFFF) {
            run++;
        }
        i += run;
        rle[out++] = (uint8_t)(run & 0xFF);
        rle[out++] = (uint8_t)(run >> 8);
        rle[out++] = (uint8_t)(color & 0xFF);
        rle[out++] = (uint8_t)(color >> 8);
    }

    const uint8_t *payload;
    uint32_t payload_len;
    const char *fmt;
    if (out + out / 2 < PET_SHOT_BYTES) { /* RLE saves enough (base64 = x4/3) */
        payload = rle;
        payload_len = out;
        fmt = "rle16";
    } else {
        payload = s_shot_buf;
        payload_len = PET_SHOT_BYTES;
        fmt = "rgb565";
    }

    shellPrint(shellGetCurrent(), "PETSHOT %d %d %s %u\n", PET_SHOT_W, PET_SHOT_H, fmt,
               (unsigned)payload_len);
    static char line[1080];
    int line_len = 0;
    for (uint32_t i = 0; i < payload_len; i += 3) {
        uint32_t v = (uint32_t)payload[i] << 16;
        if (i + 1 < payload_len) v |= (uint32_t)payload[i + 1] << 8;
        if (i + 2 < payload_len) v |= payload[i + 2];
        line[line_len++] = k_b64[(v >> 18) & 0x3F];
        line[line_len++] = k_b64[(v >> 12) & 0x3F];
        line[line_len++] = (i + 1 < payload_len) ? k_b64[(v >> 6) & 0x3F] : '=';
        line[line_len++] = (i + 2 < payload_len) ? k_b64[v & 0x3F] : '=';
        if (line_len >= (int)sizeof(line) - 4) {
            line[line_len] = '\0';
            shellPrint(shellGetCurrent(), "B64:%s\n", line);
            line_len = 0;
        }
    }
    if (line_len > 0) {
        line[line_len] = '\0';
        shellPrint(shellGetCurrent(), "B64:%s\n", line);
    }
    shellPrint(shellGetCurrent(), "PETSHOT END\n");
    return 0;
}

/* refresh the screen right after a mutation (no 1 s timer latency) */
static void pet_kick(void)
{
    pet_ui_kick();
}

static const char *const k_stat_names[4] = {"sat", "hap", "cln", "ene"};
static const char *const k_stat_full[4] = {"satiety", "happy", "clean", "energy"};
static const char *const k_stage_names[3] = {"egg", "baby", "adult"};
static const char *const k_mood_names[] = {
    "egg_calm", "egg_wobble", "sleep", "sick", "hungry",
    "sad", "dirty", "tired", "content", "normal",
};

static void pet_shell_print_state(void)
{
    pet_snapshot_t s;
    pet_core_snapshot(&s);

    shellPrint(shellGetCurrent(),
               "pet: stage=%s day=%u care=%u mood=%s%s%s wall=%d\n",
               k_stage_names[s.stage], s.age_days, s.care_count,
               k_mood_names[s.mood], s.sleeping ? " sleeping" : "",
               s.sick ? " sick" : "", s.wall_valid);

    shellPrint(shellGetCurrent(),
               "     sat=%d hap=%d cln=%d ene=%d poops=%d crack=%d\n",
               s.satiety, s.happy, s.clean, s.energy, s.poops, s.egg_crack);
}

static void pet_shell_usage(void)
{
    shellPrint(shellGetCurrent(),
               "usage: pet | pet set/add <sat|hap|cln|ene> <n> | pet stage <egg|baby|adult>\n"
               "       pet sleep|sick <0|1> | pet poop <0-3> | pet eggage/care/age/good <n>\n"
               "       pet time <seconds> | pet act <feed|snack|clean|play|light|wake|medicine|status|greet|love|pat>\n"
               "       pet evt <eat|clean|play|medicine|reject|hatch|evolve|slept|woke|greet|love|pat|rhungry|rsad|rdirty|rtired|rsick|rmiss>\n"
               "       pet save | pet reset\n");
}

static int pet_stat_idx(const char *name)
{
    for (int i = 0; i < 4; i++) {
        if (strcmp(name, k_stat_names[i]) == 0 || strcmp(name, k_stat_full[i]) == 0) {
            return i;
        }
    }
    return -1;
}

struct evt_name {
    const char *name;
    uint32_t bits;
};

static const struct evt_name k_evts[] = {
    {"eat", PET_EVT_EAT},         {"clean", PET_EVT_CLEAN},
    {"play", PET_EVT_PLAY},       {"medicine", PET_EVT_MEDICINE},
    {"reject", PET_EVT_REJECT},   {"hatch", PET_EVT_HATCH},
    {"evolve", PET_EVT_EVOLVE},   {"slept", PET_EVT_SLEPT},
    {"woke", PET_EVT_WOKE},       {"greet", PET_EVT_GREET},
    {"love", PET_EVT_LOVE},       {"pat", PET_EVT_PAT},
    {"rhungry", PET_REMIND_HUNGRY}, {"rsad", PET_REMIND_SAD},
    {"rdirty", PET_REMIND_DIRTY},  {"rtired", PET_REMIND_TIRED},
    {"rsick", PET_REMIND_SICK},    {"rmiss", PET_REMIND_MISS},
};

static int pet_cmd_dispatch(int argc, char *argv[])
{
    if (argc < 2) {
        pet_shell_print_state();
        return 0;
    }
    const char *cmd = argv[1];

    if (strcmp(cmd, "st") == 0 || strcmp(cmd, "status") == 0) {
        pet_shell_print_state();
        return 0;
    }

    if (strcmp(cmd, "set") == 0 || strcmp(cmd, "add") == 0) {
        if (argc < 4) {
            pet_shell_usage();
            return -1;
        }
        int idx = pet_stat_idx(argv[2]);
        int val = atoi(argv[3]);
        if (idx < 0) {
            shellPrint(shellGetCurrent(), "pet: unknown stat '%s'\n", argv[2]);
            return -1;
        }
        pet_snapshot_t s;
        pet_core_snapshot(&s);
        int cur[4] = {s.satiety, s.happy, s.clean, s.energy};
        int target = (cmd[0] == 's') ? val : cur[idx] + val;
        if (target < 0) {
            target = 0;
        }
        if (target > 100) {
            target = 100;
        }
        if (pet_core_debug_set_stat(idx, target) == 0) {
            shellPrint(shellGetCurrent(), "pet: %s -> %d\n", k_stat_full[idx], target);
            return 0;
        }
        return -1;
    }

    if (strcmp(cmd, "stage") == 0 && argc >= 3) {
        for (int i = 0; i < 3; i++) {
            if (strcmp(argv[2], k_stage_names[i]) == 0) {
                if (pet_core_debug_set_stage(i) == 0) {
                    shellPrint(shellGetCurrent(), "pet: stage -> %s\n", k_stage_names[i]);
                    return 0;
                }
                return -1;
            }
        }
        shellPrint(shellGetCurrent(), "pet: unknown stage '%s'\n", argv[2]);
        return -1;
    }

    if ((strcmp(cmd, "sleep") == 0 || strcmp(cmd, "sick") == 0) && argc >= 3) {
        int field = (cmd[0] == 's' && cmd[1] == 'l') ? PET_DBG_SLEEPING : PET_DBG_SICK;
        if (pet_core_debug_set_field(field, atoi(argv[2])) == 0) {
            shellPrint(shellGetCurrent(), "pet: %s -> %s\n", cmd, atoi(argv[2]) ? "on" : "off");
            return 0;
        }
        return -1;
    }

    if (strcmp(cmd, "poop") == 0 && argc >= 3) {
        if (pet_core_debug_set_field(PET_DBG_POOPS, atoi(argv[2])) == 0) {
            shellPrint(shellGetCurrent(), "pet: poops -> %d\n", atoi(argv[2]));
            return 0;
        }
        return -1;
    }

    if (strcmp(cmd, "eggage") == 0 && argc >= 3) {
        pet_core_debug_set_field(PET_DBG_EGG_AGE_S, atoi(argv[2]));
        pet_shell_print_state();
        return 0;
    }
    if (strcmp(cmd, "care") == 0 && argc >= 3) {
        pet_core_debug_set_field(PET_DBG_CARE_COUNT, atoi(argv[2]));
        pet_shell_print_state();
        return 0;
    }
    if (strcmp(cmd, "age") == 0 && argc >= 3) {
        pet_core_debug_set_field(PET_DBG_AGE_DAYS, atoi(argv[2]));
        pet_shell_print_state();
        return 0;
    }
    if (strcmp(cmd, "good") == 0 && argc >= 3) {
        pet_core_debug_set_field(PET_DBG_GOOD_DAYS, atoi(argv[2]));
        pet_shell_print_state();
        return 0;
    }

    if (strcmp(cmd, "time") == 0 && argc >= 3) {
        uint32_t sec = (uint32_t)strtoul(argv[2], NULL, 0);
        if (pet_core_debug_time_travel(sec) == 0) {
            shellPrint(shellGetCurrent(), "pet: +%u s elapsed\n", sec);
            pet_shell_print_state();
            return 0;
        }
        shellPrint(shellGetCurrent(), "pet: time failed (range 1..%u s)\n", 48u * 3600u);
        return -1;
    }

    if (strcmp(cmd, "act") == 0 && argc >= 3) {
        struct {
            const char *name;
            uint8_t action;
            uint8_t item;
        } acts[] = {
            {"feed", PET_ACTION_FEED, PET_ITEM_MEAL},
            {"snack", PET_ACTION_FEED, PET_ITEM_SNACK},
            {"clean", PET_ACTION_CLEAN, PET_ITEM_MEAL},
            {"play", PET_ACTION_PLAY, PET_ITEM_MEAL},
            {"light", PET_ACTION_LIGHT, PET_ITEM_LIGHT_SLEEP},
            {"wake", PET_ACTION_LIGHT, PET_ITEM_LIGHT_WAKE},
            {"medicine", PET_ACTION_MEDICINE, PET_ITEM_MEAL},
            {"status", PET_ACTION_STATUS, PET_ITEM_MEAL},
            {"greet", PET_ACTION_GREET, PET_ITEM_MEAL},
            {"love", PET_ACTION_LOVE, PET_ITEM_MEAL},
            {"pat", PET_ACTION_PAT, PET_ITEM_MEAL},
        };
        for (size_t i = 0; i < sizeof(acts) / sizeof(acts[0]); i++) {
            if (strcmp(argv[2], acts[i].name) == 0) {
                pet_result_t r;
                pet_core_action(acts[i].action, acts[i].item, &r);
                pet_voice_play(r.tone_id); /* hear the line while debugging */
                shellPrint(shellGetCurrent(), "pet: act %s -> %s\n", argv[2], r.tts);
                return 0;
            }
        }
        shellPrint(shellGetCurrent(), "pet: unknown action '%s'\n", argv[2]);
        return -1;
    }

    if (strcmp(cmd, "evt") == 0 && argc >= 3) {
        for (size_t i = 0; i < sizeof(k_evts) / sizeof(k_evts[0]); i++) {
            if (strcmp(argv[2], k_evts[i].name) == 0) {
                if (pet_core_debug_evt(k_evts[i].bits) == 0) {
                    shellPrint(shellGetCurrent(), "pet: evt %s raised (consumed by UI)\n",
                               argv[2]);
                    return 0;
                }
                return -1;
            }
        }
        shellPrint(shellGetCurrent(), "pet: unknown event '%s'\n", argv[2]);
        return -1;
    }

    if (strcmp(cmd, "tone") == 0 && argc >= 3) {
        int id = (int)strtol(argv[2], NULL, 0);
        if (id < 0 || id > 255) {
            shellPrint(shellGetCurrent(), "pet: tone id out of range\n");
            return -1;
        }
        pet_voice_play((uint8_t)id);
        shellPrint(shellGetCurrent(), "pet: tone %d submitted\n", id);
        return 0;
    }

    if (strcmp(cmd, "shot") == 0) {
        if (argc >= 3) {
            return pet_cmd_shot_ws(argv[2]);
        }
        return pet_cmd_shot();
    }

    if (strcmp(cmd, "save") == 0) {
        pet_core_flush_save();
        shellPrint(shellGetCurrent(), "pet: saved\n");
        return 0;
    }

    if (strcmp(cmd, "reset") == 0) {
        if (pet_core_debug_reset() == 0) {
            shellPrint(shellGetCurrent(), "pet: fresh egg\n");
            return 0;
        }
        return -1;
    }

    pet_shell_usage();
    return -1;
}

static int pet_cmd_handler(int argc, char *argv[])
{
    int rc = pet_cmd_dispatch(argc, argv);
    /* mutations from the shell should show up on screen immediately */
    if (argc >= 2) {
        pet_kick();
    }
    return rc;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 pet, pet_cmd_handler, virtual pet debug);
