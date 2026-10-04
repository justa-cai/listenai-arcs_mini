/*
 * ss_shell.c - `ss` debug shell command
 *
 *   ss                    print status
 *   ss on | ss off        enable/disable monitoring (persisted)
 *   ss server <url>       set server url (persisted, reconnects)
 *   ss events [n] [class] list newest events (default 10)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lisa_log.h"
#include "shell.h"

#include "ss_core.h"

#define TAG "ss"

static const char *const k_state_names[] = {
    [SS_STATE_OFF] = "off",
    [SS_STATE_RETRY_WAIT] = "retry-wait",
    [SS_STATE_CONNECTING] = "connecting",
    [SS_STATE_STREAMING] = "streaming",
};

static void ss_print_status(void)
{
    ss_status_t st;
    ss_core_get_status(&st);

    shellPrint(shellGetCurrent(),
               "ss: %s state=%s server=%s\n",
               st.enabled ? "ON" : "OFF", k_state_names[st.state], st.server_url);
    shellPrint(shellGetCurrent(),
               "    up=%us frames=%u drops=%u results=%u events=%u reconnects=%u\n",
               st.uptime_s, st.send_frames, st.send_drops, st.result_count, st.event_count,
               st.reconnect_count);
    shellPrint(shellGetCurrent(),
               "    probs: snoring=%u.%03u baby_cry=%u.%03u\n",
               st.last_probs_x1000[0] / 1000,
               st.last_probs_x1000[0] < 0 ? 0 : st.last_probs_x1000[0] % 1000,
               st.last_probs_x1000[1] / 1000,
               st.last_probs_x1000[1] < 0 ? 0 : st.last_probs_x1000[1] % 1000);
}

static int ss_cmd_handler(int argc, char *argv[])
{
    if (argc < 2) {
        ss_print_status();
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "on") == 0) {
        ss_core_set_enabled(true);
        shellPrint(shellGetCurrent(), "ss: monitoring ON\n");
        return 0;
    }
    if (strcmp(cmd, "off") == 0) {
        ss_core_set_enabled(false);
        shellPrint(shellGetCurrent(), "ss: monitoring OFF\n");
        return 0;
    }
    if (strcmp(cmd, "server") == 0 && argc >= 3) {
        ss_core_set_server(argv[2]);
        shellPrint(shellGetCurrent(), "ss: server -> %s\n", ss_core_get_server());
        return 0;
    }
    if (strcmp(cmd, "events") == 0) {
        uint32_t max = argc >= 3 ? (uint32_t)strtoul(argv[2], NULL, 0) : 10;
        const char *filter = argc >= 4 ? argv[3] : NULL;
        static ss_event_t events[SS_EVENT_RING];
        if (max > SS_EVENT_RING) {
            max = SS_EVENT_RING;
        }
        uint32_t n = ss_core_get_events(events, max, filter);
        shellPrint(shellGetCurrent(), "ss: %u events\n", n);
        for (uint32_t i = 0; i < n; i++) {
            shellPrint(shellGetCurrent(),
                       "  [%u] %s %s dur=%ums peak=%u%%\n",
                       i, events[i].class_name,
                       events[i].is_start ? "START" : "END",
                       events[i].duration_ms, events[i].peak_prob_x100);
        }
        return 0;
    }
    if (strcmp(cmd, "st") == 0 || strcmp(cmd, "status") == 0) {
        ss_print_status();
        return 0;
    }

    if (strcmp(cmd, "blank") == 0 && argc >= 3) {
        extern void ss_screen_test_blank(int on);
        ss_screen_test_blank(atoi(argv[2]));
        shellPrint(shellGetCurrent(), "ss: blank %s\n", atoi(argv[2]) ? "on" : "off");
        return 0;
    }

    shellPrint(shellGetCurrent(),
               "usage: ss | ss on|off | ss server <url> | ss events [n] [snoring|baby_cry]"
               " | ss blank <0|1>\n");
    return -1;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 ss, ss_cmd_handler, soundsense debug);
