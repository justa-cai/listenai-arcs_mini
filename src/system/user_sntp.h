#ifndef __LISTENAI_USER_SNTP_H__
#define __LISTENAI_USER_SNTP_H__

#define SNTP_SERVER_1       "time1.cloud.tencent.com"
#define SNTP_SERVER_2       "time2.cloud.tencent.com"

#define SNTP_SERVER_RESPONSE_TIMEOUT_MS     (3000)
#define SNTP_CLIENT_REQUEST_SEND_TIMEOUT_MS (2000)
#define SNTP_CLIENT_REQUEST_RECV_TIMEOUT_MS (1000)

struct NetworkContext
{
    int udp_socket;
};

typedef enum SntpSyncStatus
{
    SNTP_SYNC_STATUE_IDLE = 0,
    SNTP_SYNC_STATUE_SYNCING,
    SNTP_SYNC_STATUE_SYNCED,
} SntpSyncStatus_t;

typedef void (*user_sntp_status_callback)(SntpSyncStatus_t status);

void user_sntp_start(user_sntp_status_callback cb);

#endif