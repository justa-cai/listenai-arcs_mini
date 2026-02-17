#include "stdint.h"
#include "stdio.h"
#include "shell.h"
#include "cmd.h"
#include "stddef.h"
#include "string.h"
#include "lwip/opt.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_log.h"

#if LWIP_RAW
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/tcpip.h"
#include "lwip/priv/tcpip_priv.h"
#include "listen_wifi.h"

#define TAG "iperf"

typedef struct {
    void *session;
    uint8_t is_running;
} iperf_context_t;

static iperf_context_t iperf_ctx = {0};

static void iperf_report_fn(void *arg, enum lwiperf_report_type report_type,
                            const ip_addr_t* local_addr, u16_t local_port, const ip_addr_t* remote_addr, u16_t remote_port,
                            u32_t bytes_transferred, u32_t ms_duration, u32_t bandwidth_kbitpsec)
{
    const char *test_type = (arg == NULL) ? "Server" : "Client";

    LISA_LOGI(TAG, "iperf_report_fn called, test_type=%s, report_type=%d", test_type, report_type);

    switch (report_type) {
        case LWIPERF_TCP_DONE_SERVER:
        case LWIPERF_TCP_DONE_CLIENT:
            LISA_LOGI(TAG, "[%s] iperf test completed", test_type);
            printf("\n[%s] iperf test completed\n", test_type);
            printf("------------------------------------------------------------\n");
            printf("[%s] Local address: %s:%d\n", test_type, 
                   local_addr ? inet_ntoa(*local_addr) : "N/A", local_port);
            if (remote_addr && remote_port) {
                printf("[%s] Remote address: %s:%d\n", test_type, inet_ntoa(*remote_addr), remote_port);
            }
            printf("[%s] Transferred: %u bytes (%.2f MB)\n", test_type, 
                   bytes_transferred, bytes_transferred / (1024.0 * 1024.0));
            printf("[%s] Duration: %u ms (%.2f s)\n", test_type, 
                   ms_duration, ms_duration / 1000.0);
            printf("[%s] Bandwidth: %u kbit/s (%.2f Mbit/s)\n", test_type, 
                   bandwidth_kbitpsec, bandwidth_kbitpsec / 1000.0);
            printf("------------------------------------------------------------\n");
            iperf_ctx.session = NULL;
            iperf_ctx.is_running = 0;
            break;

        case LWIPERF_TCP_ABORTED_LOCAL:
            LISA_LOGW(TAG, "[%s] iperf test aborted locally", test_type);
            printf("[%s] iperf test aborted locally\n", test_type);
            iperf_ctx.session = NULL;
            iperf_ctx.is_running = 0;
            break;

        case LWIPERF_TCP_ABORTED_LOCAL_DATAERROR:
            LISA_LOGE(TAG, "[%s] iperf test aborted due to data error", test_type);
            printf("[%s] iperf test aborted due to data error\n", test_type);
            iperf_ctx.session = NULL;
            iperf_ctx.is_running = 0;
            break;

        case LWIPERF_TCP_ABORTED_LOCAL_TXERROR:
            LISA_LOGE(TAG, "[%s] iperf test aborted due to transmit error", test_type);
            printf("[%s] iperf test aborted due to transmit error\n", test_type);
            iperf_ctx.session = NULL;
            iperf_ctx.is_running = 0;
            break;

        case LWIPERF_TCP_ABORTED_REMOTE:
            LISA_LOGW(TAG, "[%s] iperf test aborted by remote", test_type);
            printf("[%s] iperf test aborted by remote\n", test_type);
            iperf_ctx.session = NULL;
            iperf_ctx.is_running = 0;
            break;

        default:
            LISA_LOGW(TAG, "[%s] iperf test unknown result: %d", test_type, report_type);
            printf("[%s] iperf test unknown result: %d\n", test_type, report_type);
            iperf_ctx.session = NULL;
            iperf_ctx.is_running = 0;
            break;
    }
}

static void iperf_start_server(void *arg)
{
    ip_addr_t local_addr;
    uint16_t local_port = (uint16_t)((uint32_t)arg);

    LISA_LOGI(TAG, "iperf_start_server called, port=%d", local_port);

    local_addr.addr = IPADDR_ANY;

    printf("Starting iperf server on port %d...\n", local_port);
    printf("Press Ctrl+C or use 'iperf stop' to stop.\n");

    LISA_LOGI(TAG, "Calling lwiperf_start_tcp_server");
    iperf_ctx.session = lwiperf_start_tcp_server(&local_addr, local_port, 
                                                   iperf_report_fn, (void*)1);

    if (iperf_ctx.session == NULL) {
        LISA_LOGE(TAG, "Failed to start iperf server");
        printf("Failed to start iperf server\n");
        iperf_ctx.is_running = 0;
    } else {
        LISA_LOGI(TAG, "iperf server started successfully, session=%p", iperf_ctx.session);
        printf("iperf server started successfully\n");
    }
}

static void iperf_start_client(void *arg)
{
    struct {
        ip_addr_t remote_addr;
        uint16_t remote_port;
        uint32_t duration;
    } *args = (void *)arg;

    LISA_LOGI(TAG, "iperf_start_client called, server=%s:%d, duration=%u", 
              inet_ntoa(args->remote_addr), args->remote_port, args->duration);

    printf("Starting iperf client...\n");
    printf("Server: %s:%d\n", inet_ntoa(args->remote_addr), args->remote_port);
    printf("Duration: %u seconds\n", args->duration);
    printf("Press Ctrl+C or use 'iperf stop' to stop.\n");

    LISA_LOGI(TAG, "Calling lwiperf_start_tcp_client");
    iperf_ctx.session = lwiperf_start_tcp_client(&args->remote_addr, args->remote_port, 
                                                   LWIPERF_CLIENT, 
                                                   iperf_report_fn, (void*)1, 
                                                   args->duration);

    if (iperf_ctx.session == NULL) {
        LISA_LOGE(TAG, "Failed to start iperf client");
        printf("Failed to start iperf client\n");
        iperf_ctx.is_running = 0;
    } else {
        LISA_LOGI(TAG, "iperf client started successfully, session=%p", iperf_ctx.session);
        printf("iperf client started successfully\n");
    }

    mem_free(args);
}

static int cmd_iperf_server(int argc, char **argv)
{
    uint16_t local_port = LWIPERF_TCP_PORT_DEFAULT;
    struct tcpip_callback_msg *msg;
    err_t err;

    LISA_LOGI(TAG, "cmd_iperf_server called, argc=%d", argc);

    if (argc > 2) {
        printf("Usage: iperf server [port]\n");
        printf("Example: iperf server\n");
        printf("         iperf server 5001\n");
        return -1;
    }

    if (argc == 2) {
        local_port = atoi(argv[1]);
        if (local_port < 1 || local_port > 65535) {
            LISA_LOGE(TAG, "Invalid port number: %d", local_port);
            printf("Invalid port number\n");
            return -1;
        }
    }

    if (iperf_ctx.is_running) {
        LISA_LOGW(TAG, "iperf session already running");
        printf("iperf session already running. Use 'iperf stop' first.\n");
        return -1;
    }

    iperf_ctx.is_running = 1;

    LISA_LOGI(TAG, "Creating tcpip callback message for iperf_start_server");
    msg = tcpip_callbackmsg_new(iperf_start_server, (void *)(uint32_t)local_port);
    if (msg == NULL) {
        LISA_LOGE(TAG, "Failed to allocate tcpip callback message");
        printf("Failed to allocate tcpip callback message\n");
        iperf_ctx.is_running = 0;
        return -1;
    }

    LISA_LOGI(TAG, "Scheduling iperf server start callback");
    err = tcpip_callbackmsg_trycallback(msg);
    if (err != ERR_OK) {
        LISA_LOGE(TAG, "Failed to schedule iperf server start: err=%d", err);
        printf("Failed to schedule iperf server start: err=%d\n", err);
        tcpip_callbackmsg_delete(msg);
        iperf_ctx.is_running = 0;
        return -1;
    }

    LISA_LOGI(TAG, "cmd_iperf_server success");
    return 0;
}

static int cmd_iperf_client(int argc, char **argv)
{
    struct {
        ip_addr_t remote_addr;
        uint16_t remote_port;
        uint32_t duration;
    } *args;
    struct tcpip_callback_msg *msg;
    err_t err;

    LISA_LOGI(TAG, "cmd_iperf_client called, argc=%d", argc);

    if (argc < 2 || argc > 4) {
        printf("Usage: iperf client <server_ip> [port] [duration]\n");
        printf("Example: iperf client 192.168.1.100\n");
        printf("         iperf client 192.168.1.100 5001\n");
        printf("         iperf client 192.168.1.100 5001 30\n");
        return -1;
    }

    if (iperf_ctx.is_running) {
        LISA_LOGW(TAG, "iperf session already running");
        printf("iperf session already running. Use 'iperf stop' first.\n");
        return -1;
    }

    LISA_LOGI(TAG, "Allocating memory for iperf client args");
    args = mem_malloc(sizeof(*args));
    if (args == NULL) {
        LISA_LOGE(TAG, "Failed to allocate memory for iperf client");
        printf("Failed to allocate memory for iperf client\n");
        return -1;
    }

    if (inet_aton(argv[1], &args->remote_addr) == 0) {
        LISA_LOGE(TAG, "Invalid IP address: %s", argv[1]);
        printf("Invalid IP address: %s\n", argv[1]);
        mem_free(args);
        return -1;
    }

    args->remote_port = LWIPERF_TCP_PORT_DEFAULT;
    if (argc >= 3) {
        args->remote_port = atoi(argv[2]);
        if (args->remote_port < 1 || args->remote_port > 65535) {
            LISA_LOGE(TAG, "Invalid port number: %d", args->remote_port);
            printf("Invalid port number\n");
            mem_free(args);
            return -1;
        }
    }

    args->duration = 10;
    if (argc >= 4) {
        args->duration = atoi(argv[3]);
        if (args->duration < 1) {
            LISA_LOGE(TAG, "Invalid duration: %u", args->duration);
            printf("Invalid duration\n");
            mem_free(args);
            return -1;
        }
    }

    iperf_ctx.is_running = 1;

    LISA_LOGI(TAG, "Creating tcpip callback message for iperf_start_client");
    msg = tcpip_callbackmsg_new(iperf_start_client, args);
    if (msg == NULL) {
        LISA_LOGE(TAG, "Failed to allocate tcpip callback message");
        printf("Failed to allocate tcpip callback message\n");
        mem_free(args);
        iperf_ctx.is_running = 0;
        return -1;
    }

    LISA_LOGI(TAG, "Scheduling iperf client start callback");
    err = tcpip_callbackmsg_trycallback(msg);
    if (err != ERR_OK) {
        LISA_LOGE(TAG, "Failed to schedule iperf client start: err=%d", err);
        printf("Failed to schedule iperf client start: err=%d\n", err);
        tcpip_callbackmsg_delete(msg);
        mem_free(args);
        iperf_ctx.is_running = 0;
        return -1;
    }

    LISA_LOGI(TAG, "cmd_iperf_client success");
    return 0;
}

static int cmd_iperf_stop(int argc, char **argv)
{
    LISA_LOGI(TAG, "cmd_iperf_stop called");

    if (iperf_ctx.session == NULL) {
        LISA_LOGW(TAG, "No active iperf session");
        printf("No active iperf session\n");
        return 0;
    }

    LISA_LOGI(TAG, "Stopping iperf session, session=%p", iperf_ctx.session);
    printf("Stopping iperf session...\n");
    lwiperf_abort(iperf_ctx.session);
    iperf_ctx.session = NULL;
    iperf_ctx.is_running = 0;
    printf("iperf session stopped\n");

    LISA_LOGI(TAG, "cmd_iperf_stop success");
    return 0;
}

static int cmd_iperf(int argc, char **argv)
{
    LISA_LOGI(TAG, "cmd_iperf called, argc=%d", argc);

    if (argc < 2) {
        printf("Usage: iperf <command> [arguments]\n");
        printf("\nCommands:\n");
        printf("  server [port]       Start iperf server (default port: 5001)\n");
        printf("  client <server_ip> [port] [duration]  Start iperf client (default port: 5001, duration: 10s)\n");
        printf("  stop                Stop running iperf session\n");
        printf("\nExamples:\n");
        printf("  iperf server           # Start server on port 5001\n");
        printf("  iperf client 192.168.1.100  # Connect to server\n");
        printf("  iperf stop             # Stop current session\n");
        return -1;
    }

    LISA_LOGI(TAG, "iperf command: %s", argv[1]);

    if (strcmp(argv[1], "server") == 0) {
        return cmd_iperf_server(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "client") == 0) {
        return cmd_iperf_client(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "stop") == 0) {
        return cmd_iperf_stop(argc - 1, argv + 1);
    } else {
        LISA_LOGW(TAG, "Unknown command: %s", argv[1]);
        printf("Unknown command: %s\n", argv[1]);
        printf("Use 'iperf' without arguments for help\n");
        return -1;
    }
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, iperf, cmd_iperf, "iperf network performance test");

#endif
