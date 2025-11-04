/**
 ****************************************************************************************
 *
 * @file cli_tcpip.c
 *
 * @brief Cli cmd for tcpip
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include "atcmd.h"
#include "ls_event.h"
#include "ls_wifi_type.h"
#include "ls_err.h"
#include "wifi_api.h"
#include "nvs.h"
#include "nvds_tag_def.h"

// dino add
#include  "rtos_al.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"
#include "atcmd_tcpip.h"
#include "atcmd_socket_test.h"
#include "log_print.h"

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
int ssc_soc(int type,char *params);
static int tcpip_cli_help(char *params);

char *pre_def_string = "empty_string";

ssc_mutex_t ssc_tcpip_mutex;
ssc_mutex_t ssc_uart_mutex;
ssc_tcpip_entity g_ssc_tcpip_table[SSC_MAX_SOC_ALLOWED];


void ssc_tcpip_init(void)
{
    ssc_tcpip_mutex = ssc_mutex_create();
    ssc_uart_mutex = ssc_mutex_create();

    ssc_os_memset(g_ssc_tcpip_table, 0, sizeof(g_ssc_tcpip_table));
    ssc_tcpip_init_internal();

}

void ssc_print_log(const char *format, ...) {
    char buffer[128];
    va_list args;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (xSemaphoreTake(ssc_uart_mutex, portMAX_DELAY)) {
#ifdef CONFIG_USE_RTT
#include "SEGGER_RTT.h"
    SEGGER_RTT_vprintf(0, format, &ap);
#else
    tfp_vprintf(format, args);
#endif
    xSemaphoreGive(ssc_uart_mutex);
    }
}
/// Array of supported CLI command
// const atcmd_item_t tcpip_commands[2] =
// {
//     {tcpip_cli_help, "soc?", "tcpip test\r\n"},
//     {ssc_soc, "soc", "soc api\r\n"}
// };

const atcmd_item_t tcpip_table[] =
{
   {ssc_soc, "soc", "soc api\r\n"}
};

void tcpip_cmd_handler(char* command, int len)
{
    uint32_t res;
    char *param;
    const atcmd_item_t *cmd;

    param = strchr(command, ' ');
    if (param)
    {
        *param++ = '\0';
        while (*param == ' ')
            param++;
    }
    else                                    
    {
        /* be sure to have \0 in command */
        command[len - 1] = '\0';
    }

    cmd = tcpip_table;
    while (cmd->atcmd_entry.func)
    {
        if (!strcmp(command, cmd->atcmd_entry.name))
            break;
        cmd++;
    }

    if ((cmd != NULL) && (cmd->atcmd_entry.func))
    {
        printf("command1 is %s\n",param);
        res = (int)cmd->atcmd_entry.func(0,param);
        /* Add default response */
        atcmd_rspinfor("%s", atcmd_res_str[res]);
    }
    else
    {
       atcmd_rspinfor("%s", atcmd_res_str[ATCMD_UNKNOWN]);
    }
}

// static int tcpip_cli_help(char *params)
// {
//     uint8_t i = 0;
//     for (i = 0; i < sizeof(tcpip_commands) / sizeof(tcpip_commands[0]); i++) {
//         ssc_print(" - %s %s\n",tcpip_commands[i].name, tcpip_commands[i].params);
//     }
//     return CLI_SUCCESS;
// }

// uint32_t tcpip_cmd_handler(char* command, int len)
// {
//     uint32_t res;
//     char *param;
//     const struct cli_cmd *cmd;

//     param = strchr(command, ' ');
//     if (param)
//     {
//         *param++ = '\0';
//         while (*param == ' ')
//             param++;
//     }
//     else
//     {
//         /* be sure to have \0 in command */
//         command[len - 1] = '\0';
//     }

//     cmd = tcpip_commands;
//     while (cmd->exec)
//     {
//         if (!strcmp(command, cmd->name))
//             break;
//         cmd++;
//     }

//     if (cmd->exec)
//     {
//         res = (uint32_t)cmd->exec(param);
//         /* Add default response */
//         if (res == CLI_SHOW_USAGE)
//         {
//             CLI_LOG("Usage:\n%s %s\n",
//                         cmd->name, cmd->params);
//         }
//     }
//     else
//     {
//         res = CLI_UNKNOWN_CMD;
//     }

//     return res;
// }

int optidx = 0;
int ssc_parse_param(char *pLine, char *argv[])
{
    int nargs = 0;

    optidx = 0;

    while (nargs < MAX_LINE_N) {
        while ((*pLine == ' ') || (*pLine == '\t')) {
            ++pLine;
        }

        if (*pLine == '\0') {
            argv[nargs] = NULL;
            return (nargs);
        }

        argv[nargs++] = pLine;

        while (*pLine && (*pLine != ' ') && (*pLine != '\t')) {
            ++pLine;
        }

        if (*pLine == '\0') {
            argv[nargs] = NULL;
            return (nargs);
        }

        *pLine++ = '\0';
    }

    return (nargs);
}

int ssc_getopt(int argc, char *const argv[], const char *optstring, char **optarg)
{
    static int optchr = 1;
    static int optopt;
    char *cp;

    /* chark input error */
    if (optchr == 1) {
        if (optidx >= argc) {
            /* all arguments processed */
            return -1;
        }

        if (argv[optidx][0] != '-' || argv[optidx][1] == '\0') {
            /* no option characters */
            return -1;
        }
    }

    if (strcmp(argv[optidx], "--") == 0) {
        /* no more options */
        optidx++;
        return -1;
    }

    optopt = argv[optidx][optchr];
    cp = (char *) strchr(optstring, optopt);

    if (cp == NULL || optopt == ':') {
        if (argv[optidx][++optchr] == '\0') {
            optchr = 1;
            optidx++;
        }

        return '?';
    }

    if (cp[1] == ':') {
        /* Argument required */
        optchr = 1;

        if (argv[optidx][optchr + 1]) {
            /* No space between option and argument */
            *optarg = &argv[optidx++][optchr + 1];
        } else if (++optidx >= argc) {
            /* option requires an argument */
            return '?';
        } else {
            /* Argument in the next argv */
            *optarg = argv[optidx++];
        }
    } else {
        /* No argument */
        if (argv[optidx][++optchr] == '\0') {
            optchr = 1;
            optidx++;
        }

        *optarg = NULL;
    }

    return *cp;
}

uint32_t ssc_getul(char *str)
{
    uint32_t ret = 0;
    unsigned char *tmp = str;

    while (isdigit(*tmp)) {
        ret = ret * 10 + *tmp++ - '0';
    }

    return ret;
}

int ssc_getsl(char *str)
{
    int ret = 0;
    bool minus = false;
    char *tmp = str;

    if (*tmp == '-') {
        minus = true;
        tmp++;
    }

    while (isdigit(*tmp)) {
        if (minus == false) {
            ret = ret * 10 + *tmp++ - '0';
        } else {
            ret = ret * 10 - (*tmp++ - '0');
        }
    }

    return ret;
}

int ssc_soc(int at_type,char *params)
{
    char *argv[CMD_MAX_ARG], pLine[CMD_BUFFER_LEN], argc, *optarg;
    char *domain = pre_def_string;
    char *multicast_ip_addr = pre_def_string;
    char temp[16];
    char how = 'B';
    uint8_t type = SSC_SOC_TYPE_NONE;
    uint8_t action = SSC_SOC_ACTION_NONE;
    uint8_t option = 0;
    uint8_t start_workthread = 1;
    uint16_t port = 0;
    uint16_t len = 1460;
    uint32_t select_time = 0;
    uint32_t send_count = 1;
    uint32_t interval = 0;
    uint16_t rate = 0;
    struct ip_total ip_addr;
    memset(&ip_addr, 0, sizeof(ip_addr));
    struct test_param param = {0};
    int ch;
    int socket_id = -1;

    strcpy(pLine, params);
    // printf("pLine is %s\n",pLine);
    argc = ssc_parse_param(pLine, argv);
    atcmd_rspinfor("pLine is %s\n",pLine);

    atcmd_rspinfor("enter ssc_soc\n");

    for (;;) {
        ch = ssc_getopt(argc, argv, "t:i:p:s:l:n:h:o:f:d:j:k:r:v:w:m:u:ABCLSDTWIHRQVMJGU", &optarg);

        if (ch < 0) {
            break;
        }

        switch (ch) {
        case 't':  //type: udp or tcp or ssl
            ssc_tcpip_trace("ssc_soc, got type\n");
            if (ssc_os_strcmp(optarg, "TCP") == 0) {
                type = SSC_SOC_TCP;
            } else if (ssc_os_strcmp(optarg, "UDP") == 0) {
                type = SSC_SOC_UDP;
            } else if (ssc_os_strcmp(optarg, "SSL") == 0) {
                type = SSC_SOC_SSL;
            } else if (ssc_os_strcmp(optarg, "TCPv6") == 0) {
                type = SSC_SOC_TCP_IPV6;
            } else if (ssc_os_strcmp(optarg, "UDPv6") == 0) {
                type = SSC_SOC_UDP_IPV6;
            } else {
                ssc_tcpip_trace("ssc socket bind error type: %s\r", optarg);
            }
            break;
        case 'i':  //ip
            ssc_tcpip_trace("ssc_soc, got ip\n");
            if (*optarg) {
                ip_addr.ip = ipaddr_addr(optarg);
            }
            break;
        case 'm':  //mutilcast ip
            ssc_tcpip_trace("ssc_soc, got multi_cast ip\n");
            multicast_ip_addr = optarg;
            break;
        case 'l':  //send length
            ssc_tcpip_trace("ssc_soc, got send len\n");
            len = ssc_getul(optarg);
            break;
        case 'k':  //if loop back
            ssc_tcpip_trace("ssc_soc, got loopback param\n");
            param.loop_back = ssc_getul(optarg);
            break;
        case 'n':  //send count
            ssc_tcpip_trace("ssc_soc, got send count\n");
            send_count = ssc_getul(optarg);
            break;
        case 'p':  //port
            ssc_tcpip_trace("ssc_soc, got port\n");
            port = ssc_getul(optarg);
            break;
        case 's':  //socket id
            ssc_tcpip_trace("ssc_soc, got socket id\n");
            socket_id = ssc_getsl(optarg);
            break;
        case 'h':  //how to shutdown
            ssc_tcpip_trace("ssc_soc, got how\n");
            how = optarg[0];
            break;
        case 'o':  //option
            ssc_tcpip_trace("ssc_soc, got option\n");
            option = ssc_getul(optarg);
            break;
        case 'u':
            select_time = ssc_getul(optarg);
            break;
        case 'r':
            ssc_tcpip_trace("ssc_soc, got send rate");
            rate = ssc_getul(optarg);
            break;
        case 'd':
            ssc_tcpip_trace("ssc_soc, got domain\n");
            domain = optarg;
            break;
        case 'j':
            ssc_tcpip_trace("ssc_soc, got send interval\n");
            interval = ssc_getul(optarg);
            break;
        case 'w':  //start work thread
            ssc_tcpip_trace("ssc_soc, got start work thread\n");
            start_workthread = ssc_getul(optarg);
            break;
        case 'B':
            ssc_tcpip_trace("ssc_soc, action bind\n");
            action = SSC_SOC_BIND;
            break;
        case 'C':
            ssc_tcpip_trace("ssc_soc, action connect\n");
            action = SSC_SOC_CONNECT;
            break;
        case 'L':
            ssc_tcpip_trace("ssc_soc, action listen\n");
            action = SSC_SOC_LISTEN;
            break;
        case 'S':
            ssc_tcpip_trace("ssc_soc, action send\n");
            action = SSC_SOC_SEND_TO;
            break;
        case 'D':
            ssc_tcpip_trace("ssc_soc, action shutdown\n");
            action = SSC_SOC_SHUTDOWN;
            break;
        case 'T':
            ssc_tcpip_trace("ssc_soc, action close\n");
            action = SSC_SOC_CLOSE;
            break;
        case 'W':
            ssc_tcpip_trace("ssc_soc, action work thread\n");
            action = SSC_SOC_WORKTHREAD;
            break;
        case 'I':
            ssc_tcpip_trace("ssc_soc, action sock info\n");
            action = SSC_SOC_INFO;
            break;
        case 'H':
            ssc_tcpip_trace("ssc_soc, action get host by name\n");
            action = SSC_SOC_GET_ADDR_INFO;
            break;
        case 'R':
            ssc_tcpip_trace("ssc_soc, action set recv print flag\n");
            action = SSC_SOC_SET_RECV_PRINT_FLAG;
            break;
        case 'V':
            ssc_tcpip_trace("ssc_soc, action validate data\n");
            action = SSC_SOC_CONFIG_DATA_VALIDATION;
            break;
        case 'M':
            ssc_tcpip_trace("ssc_soc, action change buffer size\n");
            action = SSC_SOC_CHANGE_BUFFER_SIZE;
            break;
        case 'Q':
            ssc_tcpip_trace("ssc_soc, action query status\n");
            action = SSC_SOC_QUERY;
            break;
        case 'A':
            ssc_tcpip_trace("ssc_soc, action abort\n");
            action = SSC_SOC_ABORT;
            break;
#ifdef __SSC_SOCKET__
        case 'U':
            ssc_tcpip_trace("ssc_soc, config select time\n");
            action = SSC_SOC_CONFIG_SELECT_TIME;
            break;
#endif
#ifdef __SSC_SDK_NG__
        case 'J':
            ssc_tcpip_trace("ssc_soc, action igmp join group\n");
            action = SSC_SOC_IGMP_JOIN;
            break;
        case 'G':
            ssc_tcpip_trace("ssc_soc, action igmp handle\n");
            action = SSC_SOC_IGMP_HANDLE;
            break;
#endif
        }
    }

    switch (action) {
    case SSC_SOC_BIND:
         printf("%s,%d\n",__FUNCTION__,__LINE__);
         ssc_soc_bind(type, &ip_addr, port, start_workthread, &param);
        break;
    case SSC_SOC_CONNECT:
        ssc_soc_connect(socket_id, &ip_addr, port);
        break;
    case SSC_SOC_CLOSE:
        ssc_soc_close(socket_id, false);
        break;
    case SSC_SOC_SEND_TO:
        ssc_soc_send_to(socket_id, len, send_count, &ip_addr, port, interval, rate);
        break;
    case SSC_SOC_LISTEN:
        ssc_soc_listen(socket_id);
        break;
    case SSC_SOC_INFO:
        ssc_soc_info(socket_id);
        break;
    case SSC_SOC_GET_ADDR_INFO:
        ssc_soc_getaddrinfo(domain);
        break;
    default:
        break;
    }
    return ATCMD_OK;
}

/**
 * @}
 */
