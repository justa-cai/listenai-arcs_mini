/**
 ****************************************************************************************
 *
 * @file cli_net.h
 *
 * @brief CLI cmd for net.
 *
 * Copyright (C) ListenAI 2024 ~ 2025
 *
 ****************************************************************************************
 */
#ifndef _CLI_NET_H_
#define _CLI_NET_H_


#include "net_al.h"
#include "net_ping.h"
#include "net_iperf.h"


/*
 * INCLUDE FILES
 ****************************************************************************************
 */

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
int net_cli_ping(char *params);
#endif
#if CFG_IPERF
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
int net_cli_iperf(char *params);
#endif
int net_cli_reboot(char *params);
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
int net_cli_sigkill(char *params);

void net_task_hdl_exit(rtos_task_handle handle);
/**
 * @}
 */
#endif /* _CLI_NET_H_ */

