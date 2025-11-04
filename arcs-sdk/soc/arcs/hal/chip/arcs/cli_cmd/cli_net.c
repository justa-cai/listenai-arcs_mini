/**
 ****************************************************************************************
 *
 * @file cli_net.c
 *
 * @brief Cli cmd for net
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "arcs_ap.h"
#include "rtos_al.h"
#include "net_al.h"
#include "cli_main.h"
#include "net_ping.h"
#include "net_iperf.h"



/// Command message ID associated to RTOS Task handle
struct net_task_msg_id
{
    /// task handle
    rtos_task_handle task_handle;
    /// sigkill callback
    net_cli_sigkill_cb sigkill_cb;
    /// message id
    uint32_t msg_id;
};

/// Length of @ref task_handle_msg_id
#define NET_TASK_HANDLE_LEN 5
/// Array of message IDs associated to RTOS task handles.
static struct net_task_msg_id task_handle_msg_id[NET_TASK_HANDLE_LEN];
static uint32_t net_cli_seq = 0;

static uint32_t net_get_cli_seq(void)
{
    return ++net_cli_seq;
}

int net_cli_reboot(char *params)
{
    sys_platform_sw_full_reset();

    while(1);
    return 0;
}
/**
 ****************************************************************************************
 * @brief Search a net_task_msg_id in task_handle_msg_id by RTOS task handle
 *
 * @param[in] handle            RTOS task handle
 *
 * @return pointer to net_task_msg_id on success, NULL if error.
 ****************************************************************************************
 */
static struct net_task_msg_id *net_search_task_hdl_msg(rtos_task_handle handle)
{
    int i;
    struct net_task_msg_id *ret = NULL;

    for (i = 0; i < NET_TASK_HANDLE_LEN; i++)
    {
        if (task_handle_msg_id[i].task_handle == handle)
            ret = &task_handle_msg_id[i];
    }

    return ret;
}
/**
 ****************************************************************************************
 * @brief Search a net_task_msg_id in task_handle_msg_id by message ID
 *
 * @param[in] msg_id           Message ID
 *
 * @return pointer to net_task_msg_id on success, NULL if error.
 ****************************************************************************************
 */
static struct net_task_msg_id *net_search_task_msg_id(uint32_t msg_id)
{
    int i;
    struct net_task_msg_id *ret = NULL;

    for (i = 0; i < NET_TASK_HANDLE_LEN; i++)
    {
        if (task_handle_msg_id[i].msg_id == msg_id)
            ret = &task_handle_msg_id[i];
    }

    return ret;
}

void net_task_hdl_exit(rtos_task_handle handle)
{
    struct net_task_msg_id *task_hdl_msg_ptr = net_search_task_hdl_msg(handle);

    if (!task_hdl_msg_ptr)
        return;

    task_hdl_msg_ptr->task_handle = RTOS_TASK_NULL;
    task_hdl_msg_ptr->sigkill_cb = NULL;
}

/**
 ****************************************************************************************
 * @brief Search a free net_task_msg_id in task_handle_msg_id
 *
 * @return pointer to net_task_msg_id on success, NULL if error.
 ****************************************************************************************
 */
static struct net_task_msg_id *net_search_free_task_hdl_msg(void)
{
    return net_search_task_hdl_msg(RTOS_TASK_NULL);
}
/**
 ****************************************************************************************
 * @brief Start a dedicated 'processing' task to execute a command
 *
 * This is used to create a new task to execute the command independently of the CLI task.
 * In such case it returns CLI_NO_RESP status to indicate @ref net_cli_msg_process
 * not to send status on IPC. This will be done by the processing task once the command
 * is completed using @ref net_cli_send_cmd_cfm
 *
 * @param[in] msg_id            Message ID
 * @param[in] start_task_func   Function that starts the processing task
 * @param[in] start_task_args   Argument pointer to pass to start_task_func
 * @param[in] sigkill_func      Callback for SIGKILL signal
 *
 * @return CLI_NO_RESP if successful, CLI_ERROR otherwise.
 ****************************************************************************************
 */
static int net_cli_start_task(int msg_id, net_cli_start_task_cb start_task_func,
                                void *start_task_args, net_cli_sigkill_cb sigkill_func)
{
    struct net_task_msg_id *task_hdl_msg_ptr = net_search_free_task_hdl_msg();
    if (!task_hdl_msg_ptr)
        return CLI_ERROR;

    if ((task_hdl_msg_ptr->task_handle = start_task_func(start_task_args)))
    {
        task_hdl_msg_ptr->msg_id = msg_id;
        task_hdl_msg_ptr->sigkill_cb = sigkill_func;
    }
    else
    {
        return CLI_ERROR;
    }

    return CLI_NO_RESP;
}
/**
 ****************************************************************************************
 * @brief Process function for 'sigkill' command
 *
   @verbatim
    sigkill <cmd_id> is used to stop a running command
   @endverbatim
 *
 * @param[in] params command id
 *
 * @return CLI_SUCCESS on success and CLI_ERROR if error occurred
 ****************************************************************************************
 */
int net_cli_sigkill(char *params)
{
    struct net_task_msg_id *task_msg_id;
    char *token, *next = params, *name;
    int i, msg_id, ret;

    token = utils_next_token(&next);
    if (!token)
    {
        CLI_LOG("Current tasks:\n");
        for (i = 0; i < NET_TASK_HANDLE_LEN; i++)
        {
            CLI_LOG("task[%d] ", i);
            if (task_handle_msg_id[i].task_handle && (name=rtos_get_name_by_handle(task_handle_msg_id[i].task_handle)))
                CLI_LOG("name=%s msg_id=%d\n", name, task_handle_msg_id[i].msg_id);
            else
                CLI_LOG("\n");
        }
        return CLI_SHOW_USAGE;
    }

    msg_id = atoi(token);

    task_msg_id = net_search_task_msg_id(msg_id);
    if (task_msg_id && task_msg_id->task_handle)
    {
        CLI_LOG("Kill task: %d\n", task_msg_id->task_handle);
        ret = task_msg_id->sigkill_cb(task_msg_id->task_handle);
        task_msg_id->task_handle = RTOS_TASK_NULL;
        return ret;
    }

    return CLI_ERROR;
}
#if CFG_PING
/**
 ****************************************************************************************
 * @brief Process function for 'ping' command
 *
 * Ping command can be used to test the reachability of a host on an IP network.
 *
   @verbatim
   ping <dst_ip> [-s pksize (bytes)] [-r rate (pkt/sec)] [-d duration (sec)] [-Q tos]
   ping stop <id1> [<id2> ... <id8>]
   @endverbatim
 *
 * Note that -s, -r, -d, -t are options for ping command. We could choose any of them to
 * configure. If not configured, it will set the default values at layer net_tg_al.
 *
 * @param[in] params ping command above
 *
 * @return 0 on success and !=0 if error occurred
 ****************************************************************************************
 */
int net_cli_ping(char *params)
{
    char *token, *next = params;
    uint32_t rip = 0, ret;
    token = utils_next_token(&next);
    u32_t rate = 0, pksize = 0, duration = 0, tos = 0;
    struct net_task_msg_id *task_hdl_msg_ptr;
    bool continuous = false, background = false;
    struct net_ping_task_args args;

    if (!strcmp("stop", token))
    {
        u32_t stream_id;
        struct net_ping_stream *ping_stream;
        int ret = CLI_SUCCESS;

        token = utils_next_token(&next);
        if (!token)
            return CLI_SHOW_USAGE;

        stream_id = atoi(token);
        ping_stream = net_ping_find_stream_profile(stream_id);

        if (ping_stream)
        {
            if (ping_stream->background)
            {
                task_hdl_msg_ptr = net_search_task_hdl_msg(ping_stream->ping_handle);
                ret = CLI_NO_RESP;
            }
            net_ping_stop(ping_stream);
        }
        else
        {
            CLI_LOG("Invalid stream_id %d", stream_id);
            ret = CLI_ERROR;
        }

        return ret;
    }

    do
    {
        // Handle all options of ping command
        if (token[0] == '-')
        {
            switch (token[1])
            {
                case ('s'):
                    token = utils_next_token(&next);
                    if (!token)
                        return CLI_SHOW_USAGE;
                    pksize = atoi(token);
                    break;
                case ('r'):
                    token = utils_next_token(&next);
                    if (!token)
                        return CLI_SHOW_USAGE;
                    rate = atoi(token);
                    break;
                case ('d'):
                    token = utils_next_token(&next);
                    if (!token)
                        return CLI_SHOW_USAGE;
                    duration = atoi(token);
                    break;
                case ('Q'):
                    token = utils_next_token(&next);
                    if (!token)
                        return CLI_SHOW_USAGE;
                    tos = atoi(token);
                    break;
                case ('G'):
                    background = true;
                    break;
                case ('t'):
                    continuous = true;
                    break;
                default:
                    return CLI_SHOW_USAGE;
            }
        }
        // If it's neither options, nor IP address, then the input is wrong
        else if (utils_cli_parse_ip4(token, &rip, NULL))
        {
            CLI_LOG("Invalid IP address: %s\n", token);
            return CLI_ERROR;
        }
    } while ((token = utils_next_token(&next)));

    // IP destination should be set by the command
    if (rip == 0)
        return CLI_SHOW_USAGE;

    args.rip = rip;
    args.rate = rate;
    args.pksize = pksize;
    args.duration = duration;
    args.tos = tos;
    args.background = background;
    args.continuous = continuous;
    ret = net_cli_start_task(net_get_cli_seq(), net_ping_start, &args,
                               net_ping_sigkill_handler);

    if (ret == CLI_ERROR)
    {
        CLI_LOG("Send ping error\n");
        return ret;
    }

    if (background)
        return CLI_SUCCESS;
    else
        return CLI_NO_RESP;
}
#endif
#if CFG_IPERF
#if 0
/// Long version of iperf helf
#define iperf_long_help "\
Client/Server:\n\
  -i, --interval  #        seconds between periodic bandwidth reports\n\
  -l, --len       #[KM]    length of buffer to read or write (default 8 KB)\n\
  -p, --port      #        server port to listen on/connect to\n\
  -u, --udp                use UDP rather than TCP\n\n\
Client specific:\n\
  -b, --bandwidth #[KM]    for UDP, bandwidth to send at in bits/sec\n\
                         (default 1 Mbit/sec, implies -u)\n\
  -c, --client    <host>   run in client mode, connecting to <host>\n\
  -t, --time      #        time in seconds to transmit for (default 10 secs)\n\
  -S              #        type-of-service for outgoing packets.\n\
                           You may specify the value in hex with a '0x' prefix, in octal with a '0' prefix, or in decimal\n\
  -T              #        time-to-live for outgoing multicast packets\n\
  -w              #[KM]    TCP window size (ignored for now)\n\
Server specific:\n\
  -s, --server             run in server mode"
#endif
/**
 ****************************************************************************************
 * @brief Process function for 'iperf' command
 *
 * Start an iperf server on the specified port.
 *
 * @param[in] params parameters passed to iperf command
 * @return 0 on success and !=0 if error occurred
 ****************************************************************************************
 */
int net_cli_iperf(char *params)
{
    char conv, *token, *substr, *next = params;
    struct net_iperf_settings iperf_settings;
    bool client_server_set = 0;

    net_iperf_settings_init(&iperf_settings);

    while ((token = utils_next_token(&next)))
    {
        if (token[0] != '-')
            return CLI_SHOW_USAGE;

        //TODO[AAL]: Add support long options

        switch (token[1])
        {
            case ('b'): // UDP bandwidth
            case ('w'): // TCP window size
            {
                char *decimal_str;
                int decimal = 0;
                char opt = token[1];
                uint64_t value;

                if (!(token = utils_next_token(&next)))
                {
                	CLI_LOG("%s", iperf_long_help);
                    return CLI_ERROR;
                }

                decimal_str = strchr(token, '.');
                if (decimal_str)
                {
                    int fact = 100;
                    decimal_str++;
                    while (*decimal_str >= '0' && *decimal_str <= '9')
                    {
                        decimal += (*decimal_str - '0') * fact;
                        if (fact == 1)
                            break;
                        fact = fact / 10;
                        decimal_str++;
                    }
                }

                value = atoi(token);
                conv = token[strlen(token) - 1];

                // convert according to [Gg Mm Kk]
                switch (conv)
                {
                    case 'G':
                    case 'g':
                        value *= 1000000000;
                        value += decimal * 1000000;
                        break;
                    case 'M':
                    case 'm':
                        value *= 1000000;
                        value += decimal * 1000;
                        break;
                    case 'K':
                    case 'k':
                        value *= 1000;
                        value += decimal;
                        break;
                    default:
                        break;
                }

                if (opt == 'b')
                {
                    iperf_settings.udprate = value;
                    iperf_settings.flags.is_udp = true;
                    iperf_settings.flags.is_bw_set = true;
                    // if -l has already been processed, is_buf_len_set is true so don't
                    // overwrite that value.
                    if (!iperf_settings.flags.is_buf_len_set)
                        iperf_settings.buf_len = NET_IPERF_DEFAULT_UDPBUFLEN;
                }
                else
                {
                    // TCP window is ignored for now
                }
                break;
            }
            case ('c'): // Client mode with server host to connect to
            {
                if (client_server_set)
                    return CLI_SHOW_USAGE;

                iperf_settings.flags.is_server = 0;
                client_server_set = true;

                if (!(token = utils_next_token(&next)))
                    return CLI_SHOW_USAGE;

                if (utils_cli_parse_ip4(token, &iperf_settings.host_ip, NULL))
                {
                    CLI_LOG("invalid IP address %s\n", token);
                    return CLI_ERROR;
                }
                break;
            }
            case ('f'): // format to print in
            {
                if (!(token = utils_next_token(&next)))
                {
                	CLI_LOG("%s", iperf_long_help);
                    return CLI_ERROR;
                }
                iperf_settings.format = token[0];
                break;
            }
            case ('i'): // Interval between periodic reports
            {
                uint32_t interval = 0;
                if (!(token = utils_next_token(&next)))
                {
                	CLI_LOG("%s", iperf_long_help);
                    return CLI_ERROR;
                }

                substr = strchr(token, '.');

                if (substr)
                {
                    *substr++ = '\0';
                    interval += atoi(substr);
                }

                interval += atoi(token) * 10;
                if (interval < 5)
                {
                    CLI_LOG("interval must be greater than or "
                                "equal to 0.5. Interval set to 0.5\n");
                    interval = 5;
                }

                iperf_settings.interval.sec = interval / 10;
                interval -= iperf_settings.interval.sec * 10;
                iperf_settings.interval.usec = 100000 * interval;
                iperf_settings.flags.show_int_stats = true;
                break;
            }
            case ('l'): //Length of each buffer
            {
                uint32_t udp_min_size = sizeof(struct iperf_UDP_datagram);

                if (!(token = utils_next_token(&next)))
                {
                	CLI_LOG("%s", iperf_long_help);
                    return CLI_ERROR;
                }

                iperf_settings.buf_len = atoi(token);
                iperf_settings.flags.is_buf_len_set = true;
                if (iperf_settings.flags.is_udp &&
                    (iperf_settings.buf_len < udp_min_size))
                {
                    iperf_settings.buf_len = udp_min_size;
                    CLI_LOG("buffer length must be greater than or "
                                "equal to %d in UDP\n", udp_min_size);
                }
                break;
            }
            case ('n'): // amount mode (instead of time mode)
            {
                iperf_settings.flags.is_time_mode = false;
                if (!(token = utils_next_token(&next)))
                {
                	CLI_LOG("%s", iperf_long_help);
                    return CLI_ERROR;
                }
                iperf_settings.amount = atoi(token);
                break;
            }
            case ('p'): //server port
            {
                if (!(token = utils_next_token(&next)))
                {
                	CLI_LOG("%s", iperf_long_help);
                    return CLI_ERROR;
                }
                iperf_settings.port = atoi(token);
                break;
            }
            case ('s'): // server mode
            {
                if (client_server_set)
                    return CLI_SHOW_USAGE;
                iperf_settings.flags.is_server = 1;
                client_server_set = true;
                break;
            }
            case ('t'): // time mode (instead of amount mode)
            {
                iperf_settings.flags.is_time_mode = true;
                if (!(token = utils_next_token(&next)))
                {
                	CLI_LOG("%s", iperf_long_help);
                    return CLI_ERROR;
                }
                iperf_settings.amount = 0;
                substr = strchr(token, '.');
                if (substr)
                {
                    *substr++ = '\0';
                    iperf_settings.amount += atoi(substr);
                }

                iperf_settings.amount += atoi(token) * 10;
                break;
            }
            case ('u'): // UDP instead of TCP
            {
                // if -b has already been processed, UDP rate will be non-zero, so don't
                // overwrite that value
                if (!iperf_settings.flags.is_udp)
                {
                    iperf_settings.flags.is_udp = true;
                    iperf_settings.udprate = NET_IPERF_DEFAULT_UDPRATE;
                }

                // if -l has already been processed, is_buf_len_set is true, so don't
                // overwrite that value.
                if (!iperf_settings.flags.is_buf_len_set)
                {
                    iperf_settings.buf_len = NET_IPERF_DEFAULT_UDPBUFLEN;
                }
                break;
            }
            case ('S'): // IP type-of-service
            {
                if (!(token = utils_next_token(&next)))
                {
                	CLI_LOG("%s", iperf_long_help);
                    return CLI_ERROR;
                }
                // the zero base allows the user to specify
                // hexadecimals: "0x#"
                // octals: "0#"
                // decimal numbers: "#"
                iperf_settings.tos = strtol(token, NULL, 0);
                break;
            }
            case ('T'): // TTL
            {
                if (!(token = utils_next_token(&next)))
                {
                	CLI_LOG("%s", iperf_long_help);
                    return CLI_ERROR;
                }
                iperf_settings.ttl = atoi(token);
                break;
            }
            case ('X'): // Peer version detect
            {
                iperf_settings.flags.is_peer_ver = true;
                break;
            }
            case ('h'): // Long Help
            {
            	CLI_LOG("%s", iperf_long_help);
                return CLI_SUCCESS;
            }
            default:
            {
                return CLI_SHOW_USAGE;
            }
        }
    }

    if (!client_server_set)
        return CLI_SHOW_USAGE;

    return net_cli_start_task(net_get_cli_seq(), net_iperf_start, &iperf_settings,
                                net_iperf_sigkill_handler);
}
#endif

