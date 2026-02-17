#include "stdint.h"
#include "stdio.h"
#include "shell.h"
#include "cmd.h"
#include "stddef.h"
#include "string.h"
#include "lwip/opt.h"

#if LWIP_RAW
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "lwip/dns.h"
#include "lwip/mem.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/ip4.h"
#include "lwip/sockets.h"

#define PING_RCV_TIMEO 5000
#define PING_DELAY     1000
#define PING_ID        0xAFAF
#define PING_DATA_SIZE 32

static const ip_addr_t* ping_target;
static u16_t ping_seq_num;
static u32_t ping_start_time;

static void ping_prepare_echo(struct icmp_echo_hdr *iecho, u16_t len)
{
    size_t i;
    size_t data_len = len - sizeof(struct icmp_echo_hdr);

    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0;
    iecho->id     = PING_ID;
    iecho->seqno  = lwip_htons(++ping_seq_num);

    for(i = 0; i < data_len; i++) {
        ((char*)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)i;
    }

    iecho->chksum = inet_chksum(iecho, len);
}

static err_t ping_send(int s, const ip_addr_t *addr)
{
    int err;
    struct icmp_echo_hdr *iecho;
    struct sockaddr_in to;
    size_t ping_size = sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE;

    iecho = (struct icmp_echo_hdr *)mem_malloc((mem_size_t)ping_size);
    if (!iecho) {
        return ERR_MEM;
    }

    ping_prepare_echo(iecho, (u16_t)ping_size);

    to.sin_len    = sizeof(to);
    to.sin_family = AF_INET;
    inet_addr_from_ip4addr(&to.sin_addr, ip_2_ip4(addr));

    err = lwip_sendto(s, iecho, ping_size, 0, (struct sockaddr*)&to, sizeof(to));

    mem_free(iecho);

    return (err ? ERR_OK : ERR_VAL);
}

static int ping_recv(int s)
{
    char buf[64];
    int len;
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    int ping_time_recv = -1;

    while((len = lwip_recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen)) > 0) {
        if (len >= (int)(sizeof(struct ip_hdr)+sizeof(struct icmp_echo_hdr))) {
            ip_addr_t fromaddr;
            struct ip_hdr *iphdr;
            struct icmp_echo_hdr *iecho;

            iphdr = (struct ip_hdr *)buf;
            iecho = (struct icmp_echo_hdr *)(buf + (IPH_HL(iphdr) * 4));

            if ((iecho->id == PING_ID) && (iecho->seqno == lwip_htons(ping_seq_num))) {
                ping_time_recv = sys_now() - ping_start_time;
                printf("Reply from %s: seq=%u time=%d ms\n", inet_ntoa(from.sin_addr), 
                       lwip_ntohs(iecho->seqno), ping_time_recv);
                return ping_time_recv;
            }
        }
        fromlen = sizeof(from);
    }

    return -1;
}

static int ping_exec(ip_addr_t* ip, uint32_t cnt, int *received)
{
    int s;
    int ret;
    int reply_time = -1;
    uint32_t cnt_tmp = cnt;
    int ping_time = 0;
    int ping_time_total = 0;
    struct timeval timeout;

    *received = 0;

    timeout.tv_sec = PING_RCV_TIMEO / 1000;
    timeout.tv_usec = (PING_RCV_TIMEO % 1000) * 1000;

    ping_target = ip;

    s = lwip_socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    if (s < 0) {
        printf("Failed to create raw socket\n");
        return reply_time;
    }

    ret = lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (ret != 0) {
        printf("Failed to set socket timeout\n");
        lwip_close(s);
        return -1;
    }

    while (cnt_tmp--) {
        if (ping_send(s, ping_target) == ERR_OK) {
            ping_start_time = sys_now();

            ping_time = ping_recv(s);
            if (ping_time >= 0) {
                ping_time_total += ping_time;
                (*received)++;
            } else {
                printf("Request timeout for seq=%u\n", ping_seq_num);
            }

            if (cnt_tmp > 0) {
                sys_msleep(PING_DELAY);
            }
        } else {
            printf("Ping send error\n");
            lwip_close(s);
            return -1;
        }
    }

    lwip_close(s);

    if (*received > 0) {
        reply_time = ping_time_total / (*received);
    }

    return reply_time;
}

static int cmd_ping(int argc, char **argv)
{
    ip_addr_t ping_ip_addr;
    uint32_t ping_cnt = 4;
    char *domain_name;
    int err;
    int received;

    if (argc < 2 || argc > 3) {
        printf("Usage: ping <IP or domain> [count]\n");
        printf("Example: ping 8.8.8.8 4\n");
        return -1;
    }

    if (inet_aton(argv[1], &ping_ip_addr) == 0) {
        domain_name = argv[1];
#if LWIP_DNS
        err = dns_gethostbyname(domain_name, &ping_ip_addr, NULL, NULL);
        if (err != ERR_OK) {
            printf("Failed to resolve host '%s' (error: %d)\n", domain_name, err);
            return -1;
        }
        printf("Resolved '%s' to %s\n", domain_name, inet_ntoa(ping_ip_addr));
#else
        printf("DNS is not enabled\n");
        return -1;
#endif
    }

    if (argc >= 3) {
        ping_cnt = atoi(argv[2]);
        if (ping_cnt <= 0) {
            printf("Invalid ping count\n");
            return -1;
        }
    }

    printf("PING %s (%s): %u data bytes\n", 
           argv[1], inet_ntoa(ping_ip_addr), PING_DATA_SIZE);

    int time = ping_exec(&ping_ip_addr, ping_cnt, &received);
    if (time >= 0) {
        printf("\n--- %s ping statistics ---\n", argv[1]);
        printf("%u packets transmitted, %u packets received, %.0f%% packet loss\n",
               ping_cnt, received, 
               ((float)(ping_cnt - received) / ping_cnt) * 100);
        printf("Average round-trip time: %d ms\n", time);
    } else {
        printf("\n--- %s ping statistics ---\n", argv[1]);
        printf("%u packets transmitted, %u packets received, 100%% packet loss\n", ping_cnt, received);
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ping, cmd_ping, "ping <IP or domain> [count]");

#endif
