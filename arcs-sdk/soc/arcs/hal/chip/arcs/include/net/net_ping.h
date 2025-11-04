
#ifndef _NET_PING_H_
#define _NET_PING_H_

/**
 ****************************************************************************************
 * @defgroup NET_PING NET_PING
 * @ingroup NET_CLI
 * @brief Ping task for fully hosted firmware
 * @{
 ****************************************************************************************
 */
/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdbool.h>
#include "rtos_al.h"
#include "net_tg.h"

/*
 * DEFINITIONS
 ****************************************************************************************
 */
/// Maximum number of ping streams
#define NET_PING_MAX_STREAMS 3
/// Profile ID for ping command
#define NET_PROF_PING 7

/// Ping profile
struct net_ping_profile
{
    /// IP address of destination
    uint32_t dst_ip;
    /// Ping rate (pkts /s)
    uint32_t rate;
    /// Packet size
    uint32_t pksize;
    /// Ping duration
    uint32_t duration;
    /// TOS value to be put in the IP header
    uint16_t tos;
    bool continuous;
};

/// Ping statistics
struct net_ping_stats
{
    /// Number of TX frames sent
    uint32_t tx_frames;
    /// Number of RX frames received
    uint32_t rx_frames;
    /// Number of TX buffer bytes
    uint32_t tx_bytes;
    /// Number of RX buffer bytes
    uint32_t rx_bytes;
    /// Round trip delay (in us) of ping command
    uint32_t rt_time;
    /// Epoch time of the first ping request sent
    struct net_tg_time first_msg;
    /// Epoch time of the last ping reply received or last request sent
    /// (whichever occurs last)
    struct net_tg_time last_msg;
    /// Duration (in us) between first request and last reply
    uint32_t time;
};

/// Ping configuration structure
struct net_ping_stream
{
    /// Ping thread ID
    uint32_t id;
    /// State of ping thread (true for active, false for inactive)
    bool active;
    /// Param pointer for the net_tg_al layer
    void *arg;
    /// Ping sequence number
    uint16_t ping_seq_num;
    /// Ping latest reply received
    uint16_t ping_reply_num;
    /// Ping semaphore used to synchronize the ping thread and the NET CLI one
    rtos_semaphore ping_semaphore;
    /// Handle of ping send task (not used yet)
    rtos_task_handle ping_handle;
    /// Ping timestamp
    struct net_tg_time ping_timestamp;
    /// Ping profile
    struct net_ping_profile prof;
    /// Ping statistics
    struct net_ping_stats stats;
    /// Background task
    bool background;
};

/// Arguments for @ref net_ping_start
struct net_ping_task_args
{
    ///Destination IP address
    uint32_t rip;
    ///Ping rate (pkts / s)
    uint32_t rate;
    ///Packet size
    uint32_t pksize;
    ///Ping duration
    uint32_t duration;
    ///Type of service
    uint16_t tos;
    ///Boolean for running ping in background
    bool background;
    bool continuous;
};

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
/// Table of ping streams
extern struct net_ping_stream p_streams[NET_PING_MAX_STREAMS];

/*
 * FUNCTIONS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Search ping stream by stream id
 * It will return the ping stream associated to the provided ping stream id.
 *
 * @param[in] stream_id  Ping stream id
 *
 * @return Pointer to ping stream if found, NULL otherwise
 ****************************************************************************************
 **/
struct net_ping_stream *net_ping_find_stream_profile(uint32_t stream_id);

/**
 ****************************************************************************************
 * @brief Start ping command with certain configuration options
 * This function creates the RTOS task dedicated to the ping send processing and return
 * the stream id of ping stream launched.
 *
 * @param[in] args  Pointer to arguments for net_ping_start (@ref net_ping_task_args)
 *
 * @return RTOS task handle of ping task on success, NULL otherwise
 ****************************************************************************************
 */
rtos_task_handle net_ping_start(void *args);

/**
 ****************************************************************************************
 * @brief Stop ping process launched
 * This function stops the ping process according to ping stream
 *
 * @param[in] ping_stream Pointer to the ping stream to stop
 ****************************************************************************************
 */
void net_ping_stop(struct net_ping_stream *ping_stream);


/**
 ****************************************************************************************
 * @brief Ping sigkill handler. Closes connection and ping stream.
 *
 * @param[in] ping_handle      Ping task handle
 *
 * @return NET_CLI_SUCCESS if successful, NET_CLI_ERROR otherwise
 ****************************************************************************************
 **/
int net_ping_sigkill_handler(rtos_task_handle ping_handle);

/// @}

#endif // _NET_PING_H_
