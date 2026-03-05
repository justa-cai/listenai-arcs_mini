#define TAG "user-sntp"

#include "sysheap.h"
#include "lisa_log.h"
#include "lisa_thread.h"
#include "lisa_typedef.h"
#include "listen_system.h"
#include "Driver_RTC.h"
#include "core_sntp_client.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "listen_wifi.h"
#include "user_sntp.h"

#include "lisa_log.h"

static SntpSyncStatus_t s_sntp_sync_status = SNTP_SYNC_STATUE_IDLE;
static user_sntp_status_callback s_sntp_sync_status_cb = NULL;

static bool _user_resolve_DNS(const SntpServerInfo_t *p_server_addr, uint32_t *p_ipv4_addr)
{
    bool status = false;
    int32_t dns_status = -1;
    struct addrinfo hints;
    struct addrinfo *p_list_head = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = (int32_t)SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    dns_status = getaddrinfo(p_server_addr->pServerName, NULL, &hints, &p_list_head);
    if (dns_status == 0) {
        struct sockaddr_in *p_addr_info = (struct sockaddr_in *)p_list_head->ai_addr;
        memcpy(p_ipv4_addr, &p_addr_info->sin_addr, sizeof(uint32_t));
        status = true;
    }

    freeaddrinfo(p_list_head);

    return status;
}

static int32_t _user_udp_transport_send(NetworkContext_t *pNetworkContext,
                                  uint32_t serverAddr,
                                  uint16_t serverPort,
                                  const void *pBuffer,
                                  uint16_t bytesToSend)
{
    int32_t bytesSent = -1;

    struct sockaddr_in addrInfo;
    addrInfo.sin_family = AF_INET;
    addrInfo.sin_port = htons(serverPort);
    addrInfo.sin_addr.s_addr = serverAddr;

    bytesSent = sendto(pNetworkContext->udp_socket,
                       pBuffer,
                       bytesToSend, 0,
                       (const struct sockaddr *)&addrInfo,
                       sizeof(addrInfo));

    return bytesSent;
}

static int32_t _user_udp_transport_recv(NetworkContext_t *pNetworkContext,
                                  uint32_t serverAddr,
                                  uint16_t serverPort,
                                  void *pBuffer,
                                  uint16_t bytesToRecv)
{
    int32_t bytesReceived = -1;

    struct sockaddr_in addrInfo;
    addrInfo.sin_family = AF_INET;
    addrInfo.sin_port = htons(serverPort);
    addrInfo.sin_addr.s_addr = serverAddr;
    socklen_t addrLen = sizeof(addrInfo);

    bytesReceived = recvfrom(pNetworkContext->udp_socket, pBuffer,
                             bytesToRecv, 0,
                             (struct sockaddr *)&addrInfo,
                             &addrLen);
    return (bytesReceived <= 0 ? 0 : bytesReceived);
}

static void _user_sntp_client_set_time(const SntpServerInfo_t *pTimeServer,
                                 const SntpTimestamp_t *pServerTime,
                                 int64_t clockOffsetMs,
                                 SntpLeapSecondInfo_t leapSecondInfo)
{
    uint32_t unixSecs;
    uint32_t unixMs;
    SntpStatus_t status = Sntp_ConvertToUnixTime(pServerTime, &unixSecs, &unixMs);
    if (status == SntpSuccess) {
        struct timeval tm;
        tm.tv_sec = unixSecs;
        tm.tv_usec = unixMs;
        ls_sys_set_timeval(&tm);

        s_sntp_sync_status = SNTP_SYNC_STATUE_SYNCED;
    } else {
        LISA_LOGE(TAG, "Sntp_ConvertToUnixTime fail: %d", status);
    }
}

static void _user_sntp_client_get_time(SntpTimestamp_t *p_current_time)
{
    struct timeval tm;
    ls_sys_get_time(&tm);

    p_current_time->seconds = tm.tv_sec;
    p_current_time->fractions = (tm.tv_sec / 1000) * SNTP_FRACTION_VALUE_PER_MICROSECOND;
}

static UdpTransportInterface_t udp_transport_intf;
/* Create UDP socket. */
static NetworkContext_t udpContext;
static int _user_sntp_init(SntpContext_t *context, SntpServerInfo_t *servers, int count, uint8_t *network_buffer)
{
    int ret;
    uint32_t pollingIntervalPeriod;

    udpContext.udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udpContext.udp_socket < 0) {
		LISA_LOGE(TAG, "socket create err");
        return -1;
	}

    int val = 1;
    if ((ret = setsockopt(udpContext.udp_socket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int))) != 0) {
		LISA_LOGE(TAG, "setsockopt SO_REUSEADDR fail: %d", ret);
        if (udpContext.udp_socket) closesocket(udpContext.udp_socket);
        return -1;
	}

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
	if ((ret = setsockopt(udpContext.udp_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval))) != 0) {
        LISA_LOGE(TAG, "setsockopt SO_RCVTIMEO fail: %d", ret);
        if (udpContext.udp_socket) closesocket(udpContext.udp_socket);
        return -1;
	}

    udp_transport_intf.pUserContext = &udpContext;
    udp_transport_intf.sendTo = _user_udp_transport_send;
    udp_transport_intf.recvFrom = _user_udp_transport_recv;

    /* Initialize context. */
    SntpStatus_t status = Sntp_Init(context,
                                    servers,
                                    count,
                                    SNTP_SERVER_RESPONSE_TIMEOUT_MS,
                                    network_buffer,
                                    SNTP_PACKET_BASE_SIZE,
                                    _user_resolve_DNS,
                                    _user_sntp_client_get_time,
                                    _user_sntp_client_set_time,
                                    &udp_transport_intf,
                                    NULL);
    if (status != SntpSuccess) {
        LISA_LOGE(TAG, "sntp init fail: %d", status);
        if (udpContext.udp_socket) closesocket(udpContext.udp_socket);
        return -1;
    }

    return 0;
}

static void _user_sntp_deinit(void)
{
    if (udpContext.udp_socket > 0) {
        closesocket(udpContext.udp_socket);
    }
}

static void _user_sntp_thread_func(void *param)
{
    SntpContext_t context;
    uint8_t *network_buffer = inram_calloc(32, 1, SNTP_PACKET_BASE_SIZE);

    /* Setup list of time servers. */
    SntpServerInfo_t time_servers[] = {
        {
            .port = SNTP_DEFAULT_SERVER_PORT,
            .pServerName = SNTP_SERVER_1,
            .serverNameLen = strlen(SNTP_SERVER_1)
        },
        {
            .port = SNTP_DEFAULT_SERVER_PORT,
            .pServerName = SNTP_SERVER_2,
            .serverNameLen = strlen(SNTP_SERVER_2)
        }
    };
    // sntp server count
    int server_count = sizeof(time_servers) / sizeof(SntpServerInfo_t);
    // user sntp init
    if (_user_sntp_init(&context, time_servers, server_count, network_buffer) != 0) {
        inram_free(network_buffer);
        return;
    }
    int sntp_sync_count = 0;

    while (true) {
        if (ls_wifi_is_connected()) {
            for (int i = 0; i < server_count; i ++) {
                SntpStatus_t status = Sntp_SendTimeRequest(&context,
                                                            (rand() % UINT32_MAX),
                                                            SNTP_CLIENT_REQUEST_SEND_TIMEOUT_MS);
                do {
                    status = Sntp_ReceiveTimeResponse(&context, SNTP_CLIENT_REQUEST_RECV_TIMEOUT_MS);
                } while(status == SntpNoResponseReceived);

                sntp_sync_count++;

                if (status == SntpSuccess) {
                    break;
                }

                lisa_thread_mdelay(100);
            }
        } else {
            LOGE("network is not connected!");
        }

        if (s_sntp_sync_status == SNTP_SYNC_STATUE_SYNCED) {
            if (s_sntp_sync_status_cb) {
                s_sntp_sync_status_cb(s_sntp_sync_status);
                s_sntp_sync_status_cb = NULL;
            }
            LOGI("sntp sync success %d", sntp_sync_count);
            break;
        }
        LOGW("sntp sync failed, retry 3s later");

        lisa_thread_mdelay(3000);
    }

    inram_free(network_buffer);
    _user_sntp_deinit();

    // reset sntp sync idle
    s_sntp_sync_status = SNTP_SYNC_STATUE_IDLE;
}

void user_sntp_start(user_sntp_status_callback cb)
{
    if (s_sntp_sync_status != SNTP_SYNC_STATUE_IDLE) {
        LISA_LOGW(TAG, "sntp already started");
        return;
    }

    s_sntp_sync_status = SNTP_SYNC_STATUE_SYNCING;
    s_sntp_sync_status_cb = cb;

    if (s_sntp_sync_status_cb) {
        s_sntp_sync_status_cb(s_sntp_sync_status);
    }

    lisa_thread_attr_t att = {
        .name = "user-sntp",
        .stack_size = 4096,
        .priority = LISA_OS_PRIORITY_NORMAL,
    };
    lisa_thread_create(&att, _user_sntp_thread_func, NULL);
}