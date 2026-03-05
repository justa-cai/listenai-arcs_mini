#include <time.h>
#include <stdint.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "core_sntp_client.h"
#include <sys/time.h>
#include "lisa_sntp.h"
#include "lisa_time.h"

#define TAG "lisa-sntp"

#include "lisa_log.h"

#define TIME_REQUEST_SEND_WAIT_TIME_MS    2000
#define TIME_REQUEST_RECEIVE_WAIT_TIME_MS 2000

#ifndef offsetof
#define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE, MEMBER)
#endif

#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, field) ((type *)(((char *)(ptr)) - offsetof(type, field)))
#endif

struct NetworkContext {
	int udpSocket;
};

struct LisaSntpQueryContext {
	SntpServerInfo_t serverInfo;
	struct lisa_sntp_time *time;
	uint8_t status;
};

static bool resolveDns(const SntpServerInfo_t *pServerAddr, uint32_t *pIpV4Addr)
{
	bool status = false;

	struct addrinfo hints;
	struct addrinfo *pListHead;

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int st = getaddrinfo(pServerAddr->pServerName, NULL, &hints, &pListHead);

	if (st) {
		LISA_NLOGE("resolveDns, server name:%s, status:%d", pServerAddr->pServerName, st);
		return status;
	}

	if (st == 0) {
		struct sockaddr_in *pAddrInfo = (struct sockaddr_in *)pListHead->ai_addr;
		uint8_t ipv4_addr[32] = {0};
		*pIpV4Addr = ntohl(pAddrInfo->sin_addr.s_addr);
		inet_ntop(pAddrInfo->sin_family, &pAddrInfo->sin_addr, (int8_t *)ipv4_addr,
			  sizeof(ipv4_addr));
		status = true;
	}

	freeaddrinfo(pListHead);

	return status;
}

static int32_t UdpTransport_Send(NetworkContext_t *pNetworkContext, uint32_t serverAddr,
				 uint16_t serverPort, const void *pBuffer, uint16_t bytesToSend)
{
	int32_t bytesSent = -1;
	struct sockaddr_in addrInfo;
	addrInfo.sin_family = AF_INET;
	addrInfo.sin_port = htons(serverPort);
	addrInfo.sin_addr.s_addr = htonl(serverAddr);

	bytesSent = sendto(pNetworkContext->udpSocket, pBuffer, bytesToSend, 0,
			   (const struct sockaddr *)&addrInfo, sizeof(addrInfo));

	return bytesSent < 0 ? 0 : bytesSent;
}

static int32_t UdpTransport_Recv(NetworkContext_t *pNetworkContext, uint32_t serverAddr,
				 uint16_t serverPort, void *pBuffer, uint16_t bytesToRecv)
{
	int32_t bytesReceived = -1;

	struct sockaddr_in addrInfo;
	addrInfo.sin_family = AF_INET;
	addrInfo.sin_port = htons(serverPort);
	addrInfo.sin_addr.s_addr = htonl(serverAddr);
	socklen_t addrLen = sizeof(addrInfo);

	bytesReceived = recvfrom(pNetworkContext->udpSocket, pBuffer, bytesToRecv, 0,
				 (struct sockaddr *)&addrInfo, &addrLen);

	return bytesReceived < 0 ? 0 : bytesReceived;
}

static void sntpClient_SetTime(const SntpServerInfo_t *pTimeServer,
			       const SntpTimestamp_t *pServerTime, int64_t clockOffsetMs,
			       SntpLeapSecondInfo_t leapSecondInfo)
{
	uint32_t unixSecs;
	uint32_t unixMs;
	struct LisaSntpQueryContext *ctx =
		CONTAINER_OF(pTimeServer, struct LisaSntpQueryContext, serverInfo);

	ctx->status = Sntp_ConvertToUnixTime(pServerTime, &unixSecs, &unixMs);
	if (ctx->status == 0) {
		ctx->time->sec = unixSecs;
		ctx->time->nsec = unixMs * 1000;
	}
}

static void sntpClient_GetTime(SntpTimestamp_t *pCurrentTime)
{
	uint64_t tick_ms = lisa_os_get_tick_ms();
	uint32_t tick_secs = tick_ms / 1000;
	uint32_t tick_ms_fraction = tick_ms % 1000;

	pCurrentTime->seconds = tick_secs + SNTP_TIME_AT_UNIX_EPOCH_SECS;
	pCurrentTime->fractions = tick_ms_fraction * 1000 * SNTP_FRACTION_VALUE_PER_MICROSECOND;
}

/**
 * NOTE:
 * In order to ensure thread safety for this function under synchronized query time,
 * we have to make some restrictions here.
 */
static int lisa_sntp_query_once(const char *server, uint32_t timeout, struct lisa_sntp_time *time)
{
	uint8_t networkBuffer[SNTP_PACKET_BASE_SIZE];
	NetworkContext_t udpContext = {0};
	struct LisaSntpQueryContext lisaSntpQueryCtx = {0};
	UdpTransportInterface_t udpTransportIntf = {0};
	SntpContext_t context = {0};

	udpContext.udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (udpContext.udpSocket < 0) {
		LISA_NLOGE("invalid sntp socket:%d", udpContext.udpSocket);
		return -1;
	}
	struct timeval timeval_send = {
		.tv_sec = TIME_REQUEST_SEND_WAIT_TIME_MS / 1000,
		.tv_usec = 0,
	};
	struct timeval timeval_recv = {
		.tv_sec = TIME_REQUEST_RECEIVE_WAIT_TIME_MS / 1000,
		.tv_usec = 0,
	};

	setsockopt(udpContext.udpSocket, SOL_SOCKET, SO_SNDTIMEO, &timeval_send,
		   sizeof(timeval_send));
	setsockopt(udpContext.udpSocket, SOL_SOCKET, SO_RCVTIMEO, &timeval_recv,
		   sizeof(timeval_recv));

	lisaSntpQueryCtx.serverInfo.port = SNTP_DEFAULT_SERVER_PORT;
	lisaSntpQueryCtx.serverInfo.pServerName = server;
	lisaSntpQueryCtx.serverInfo.serverNameLen = strlen(server);
	lisaSntpQueryCtx.time = time;
	lisaSntpQueryCtx.status = -1;

	udpTransportIntf.pUserContext = &udpContext;
	udpTransportIntf.sendTo = UdpTransport_Send;
	udpTransportIntf.recvFrom = UdpTransport_Recv;

	SntpStatus_t status =
		Sntp_Init(&context, &lisaSntpQueryCtx.serverInfo, 1, timeout, networkBuffer,
			  SNTP_PACKET_BASE_SIZE, resolveDns, sntpClient_GetTime, sntpClient_SetTime,
			  &udpTransportIntf, NULL);
	if (status != SntpSuccess) {
		LISA_NLOGE("Sntp_Init() failed,status=%d", status);
		close(udpContext.udpSocket);
		return -1;
	}

	status = Sntp_SendTimeRequest(&context, lisa_rand32() % UINT32_MAX,
				      TIME_REQUEST_SEND_WAIT_TIME_MS * 2);
	if (status != SntpSuccess) {
		LISA_NLOGE("Sntp_SendTimeRequest() failed,status=%d", status);
		close(udpContext.udpSocket);
		return -1;
	}

	status = Sntp_ReceiveTimeResponse(&context, TIME_REQUEST_RECEIVE_WAIT_TIME_MS * 2);
	if (status != SntpSuccess) {
		LISA_NLOGE("Sntp_ReceiveTimeResponse failed,status=%d", status);
		close(udpContext.udpSocket);
		return -1;
	}

	close(udpContext.udpSocket);

	/* lisaSntpQueryCtx.status will update at function sntpClient_SetTime  */
	if (lisaSntpQueryCtx.status != SntpSuccess) {
		return -1;
	}

	return 0;
}

int lisa_sntp_query(const char *servers[], int servers_cnt, uint32_t timeout,
		    struct lisa_sntp_time *time)
{
	int i;
	int err = -1;

	if (time == NULL || servers == NULL || servers_cnt == 0) {
		return -1;
	}

	for (i = 0; i < servers_cnt; i++) {
		err = lisa_sntp_query_once(servers[i], timeout, time);
		if (err == 0) {
			return 0;
		}
	}

	return err;
}
