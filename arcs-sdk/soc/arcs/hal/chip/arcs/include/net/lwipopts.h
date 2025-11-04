/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Simon Goldschmidt
 *
 */
#ifndef LWIP_HDR_LWIPOPTS_H__
#define LWIP_HDR_LWIPOPTS_H__

#include <stdint.h>

#define lwip_printf
#ifndef ASSERT_ERR
#define ASSERT_ERR( x ) configASSERT( x )
#endif
uint16_t fhost_ip_chksum(const void *dataptr, int len);

#define LWIP_NETIF_API                1

#define TCPIP_MBOX_SIZE               10
#define TCPIP_THREAD_STACKSIZE        1024
#define TCPIP_THREAD_PRIO             8

#define DEFAULT_THREAD_STACKSIZE      1024
#define DEFAULT_THREAD_PRIO           1

#define DEFAULT_RAW_RECVMBOX_SIZE     32
#define DEFAULT_UDP_RECVMBOX_SIZE     32
#define DEFAULT_TCP_RECVMBOX_SIZE     32
#define DEFAULT_ACCEPTMBOX_SIZE       32

#define LWIP_NETIF_LOOPBACK           1
#define LWIP_HAVE_LOOPIF              1
#define LWIP_LOOPBACK_MAX_PBUFS       0

#define LWIP_CHKSUM_ALGORITHM         3
#if defined(CFG_AMP_IPC_TCPIP) && defined(CFG_AMP_IPC_MASTER)
#define LWIP_CHKSUM                   net_ip_chksum

#define LWIP_FUNC_ATTR                 //__attribute__ ((section (".ramcode")))
#define LWIP_FUNC_ATTR_                //__attribute__ ((section (".ramcode2")))
#define LWIP_FUNC_ALIGN                __attribute__((aligned(32)))

#else
#define LWIP_CHKSUM                   fhost_ip_chksum

#define LWIP_FUNC_ATTR
#define LWIP_FUNC_ATTR_
#define LWIP_FUNC_ALIGN

#endif
#define LWIP_TCPIP_CORE_LOCKING_INPUT 1

#ifdef TX_BUF_COPY
#if NX_WAPI
#define PBUF_LINK_ENCAPSULATION_HLEN  72
#else
#define PBUF_LINK_ENCAPSULATION_HLEN  64
#endif
#else
#if NX_WAPI
#define PBUF_LINK_ENCAPSULATION_HLEN  400
#else
#define PBUF_LINK_ENCAPSULATION_HLEN  392
#endif
#endif

#define IP_REASS_MAX_PBUFS            15

#define MEMP_NUM_NETBUF               32
#define MEMP_NUM_NETCONN              15
#define MEMP_NUM_UDP_PCB              16
#define MEMP_NUM_REASSDATA            5

#define MAC_RXQ_DEPTH                 8
#define MAC_TXQ_DEPTH                 8
#define TCP_MSS                       1460
#define TCP_WND                       (2 * MAC_RXQ_DEPTH * TCP_MSS)

#define TCP_SND_BUF                   (4 * MAC_TXQ_DEPTH * TCP_MSS)

#define TCP_QUEUE_OOSEQ               1
#define MEMP_NUM_TCP_SEG              ((4 * TCP_SND_BUF) / TCP_MSS)
#define MEMP_NUM_PBUF_NET             (TCP_SND_BUF / TCP_MSS)
#define MEMP_NUM_PBUF                 (MEMP_NUM_PBUF_NET + 20) //20 for internal msg
#define PBUF_POOL_SIZE                0
#define LWIP_WND_SCALE                1
#define TCP_RCV_SCALE                 2
#define TCP_SNDLOWAT                  LWIP_MIN(LWIP_MAX(((TCP_SND_BUF)/4),               \
                                                        (2 * TCP_MSS) + 1),              \
                                               (TCP_SND_BUF) - 1)
#ifdef AP_STA_COEXIT
#define EXTRA_MEM_SIZE                8192
#else
#define EXTRA_MEM_SIZE                0
#endif

#define MEM_MIN_TCP                   (2300 + MEMP_NUM_PBUF_NET * (100 + PBUF_LINK_ENCAPSULATION_HLEN))
#if NX_TG
#define MEM_MIN_TG                    16384
#else
#define MEM_MIN_TG                    0
#endif
#if MEM_MIN_TCP > MEM_MIN_TG
#define MEM_MIN                       MEM_MIN_TCP
#else
#define MEM_MIN                       MEM_MIN_TG
#endif
#define MEM_ALIGNMENT                 4
#if MEM_MIN > 8192
#define MEM_SIZE                      (MEM_MIN + EXTRA_MEM_SIZE) 
#else
#define MEM_SIZE                      (8192 + EXTRA_MEM_SIZE) 
#endif

#define LWIP_HOOK_FILENAME            "lwiphooks.h"

#define LWIP_RAW                      1
#define LWIP_MULTICAST_TX_OPTIONS     1

#define LWIP_TIMEVAL_PRIVATE          0  // use sys/time.h for struct timeval
//#define LWIP_PROVIDE_ERRNO          1

#define LWIP_DHCP                     1
#define LWIP_DNS                      1
#define LWIP_IGMP                     0
#define LWIP_SO_RCVTIMEO              1
#define LWIP_ACD                      0
#define LWIP_DHCP_DOES_ACD_CHECK      0
#define LWIP_NETIF_STATUS_CALLBACK    1
#define SO_REUSE                      1

#ifdef TX_BUF_COPY
#define MEM_LIBC_MALLOC   1
#if MEM_LIBC_MALLOC
#include <rtos_al.h>
#define mem_clib_malloc                 rtos_malloc
#define mem_clib_free                   rtos_free
#define mem_clib_calloc(n, m)           rtos_calloc(n, m)
#endif
#define MEMP_MEM_MALLOC   1
#endif

#if 0
/* Prevent having to link sys_arch.c (we don't test the API layers in unit tests) */
#define NO_SYS                          0
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0
#define SYS_LIGHTWEIGHT_PROT            0

#define LWIP_IPV6                       1
#define IPV6_FRAG_COPYHEADER            1
#define LWIP_IPV6_DUP_DETECT_ATTEMPTS   0

/* Turn off checksum verification of fuzzed data */
#define CHECKSUM_CHECK_IP               0
#define CHECKSUM_CHECK_UDP              0
#define CHECKSUM_CHECK_TCP              0
#define CHECKSUM_CHECK_ICMP             0
#define CHECKSUM_CHECK_ICMP6            0

/* Minimal changes to opt.h required for tcp unit tests: */
#define MEM_SIZE                        16000
#define TCP_SND_QUEUELEN                40
#define MEMP_NUM_TCP_SEG                TCP_SND_QUEUELEN
#define TCP_SND_BUF                     (12 * TCP_MSS)
#define TCP_WND                         (10 * TCP_MSS)
#define LWIP_WND_SCALE                  1
#define TCP_RCV_SCALE                   0
#define PBUF_POOL_SIZE                  400 /* pbuf tests need ~200KByte */

/* Minimal changes to opt.h required for etharp unit tests: */
#define ETHARP_SUPPORT_STATIC_ENTRIES   1
#endif

/* ---------- DHCP options ---------- */
#define DHCP_DEFINE_CUSTOM_TIMEOUTS     1
#define DHCP_COARSE_TIMER_SECS          1
#define DHCP_NEXT_TIMEOUT_THRESHOLD     (3)
/* Since for embedded devices it's not that hard to miss a discover packet, so lower
 * the discover and request retry backoff time from (2,4,8,16,32,60,60)s to (500m,500m,1,2,2,2)s.
 */
#define DHCP_REQUEST_BACKOFF_SEQUENCE(tries)   ((uint16_t)(((tries) < 5 ? ((tries) < 2 ? 2 : (1 << (tries - 1))) : 8) * 250))

/* Use custom DHCP timeout type to support longer lease times (with IDF coarse timer granularity)
 */
#define DHCP_TIMEOUT_SIZE_T             u32_t
static inline uint32_t timeout_from_offered(uint32_t lease, uint32_t min)
{
    uint32_t timeout = lease;
    if (timeout == 0) {
        timeout = min;
    }
    timeout = (timeout + DHCP_COARSE_TIMER_SECS - 1) / DHCP_COARSE_TIMER_SECS;
    return timeout;
}

#define DHCP_SET_TIMEOUT_FROM_OFFERED_T0_LEASE(tout, dhcp)  do {    \
        (tout) = timeout_from_offered((dhcp)->offered_t0_lease, 120); } while(0)
#define DHCP_SET_TIMEOUT_FROM_OFFERED_T1_RENEW(tout, dhcp)  do {    \
        (tout) = timeout_from_offered((dhcp)->offered_t1_renew, (dhcp)->t0_timeout>>1 /* 50% */ );  } while(0)
#define DHCP_SET_TIMEOUT_FROM_OFFERED_T2_REBIND(tout, dhcp) do {    \
        (tout) = timeout_from_offered((dhcp)->offered_t2_rebind, ((dhcp)->t0_timeout/8)*7 /* 87.5% */ );  } while(0)

#define LWIP_HOOK_DHCP_APPEND_OPTIONS(netif, dhcp, state, msg, msg_type, options_len_ptr) \
        dhcp_append_extra_opts(netif, state, msg, options_len_ptr);
/* ---------- End of DHCP options ---------- */

/**
 * CONFIG_LWIP_DHCP_RESTORE_LAST_IP==1: Last valid IP address obtained from DHCP server
 * is restored after reset/power-up.
 */
#include "netif/dhcp_state.h"
#define CONFIG_LWIP_DHCP_RESTORE_LAST_IP 1

#if CONFIG_LWIP_DHCP_RESTORE_LAST_IP

#define LWIP_DHCP_IP_ADDR_RESTORE()     dhcp_ip_addr_restore(netif)
#define LWIP_DHCP_IP_ADDR_STORE()       dhcp_ip_addr_store(netif)
#else
 
#define LWIP_DHCP_IP_ADDR_RESTORE()      0
#define LWIP_DHCP_IP_ADDR_STORE()        0
#endif


/**
 * LWIP_DHCP_DISABLE_CLIENT_ID==1: Do not add option 61 (client-id) to DHCP packets
 *
 */
#define CONFIG_LWIP_DHCP_CLIENT_ID      1
#if CONFIG_LWIP_DHCP_CLIENT_ID
#define LWIP_DHCP_ENABLE_CLIENT_ID      1
#else
#define LWIP_DHCP_ENABLE_CLIENT_ID      0
#endif


/*
   ------------------------------------------------
   ---------- Network Interfaces options ----------
   ------------------------------------------------
*/

/**
 * LWIP_NETIF_HOSTNAME==1: use DHCP_OPTION_HOSTNAME with netif's hostname
 * field.
 * LWIP_DHCP_DISCOVER_ADD_HOSTNAME==1: include hostname opt in discover packets.
 * If the hostname is not set in the DISCOVER packet, then some servers might issue
 * an OFFER with hostname configured and consequently reject the REQUEST with any other hostname.
 */
#define LWIP_NETIF_HOSTNAME             1

/** LWIP_DHCP_DISCOVER_ADD_HOSTNAME: Set to 1 to include hostname opt in discover packets.
 * If the hostname is not set in the DISCOVER packet, then some servers might issue an OFFER with hostname
 * configured and consequently reject the REQUEST with any other hostname.
 */
#define LWIP_DHCP_DISCOVER_ADD_HOSTNAME 1

/**
 * LWIP_SO_SNDTIMEO==1: Enable send timeout for sockets/netconns and
 * SO_SNDTIMEO processing.
 */
#define LWIP_SO_SNDTIMEO                1

/**
 * LWIP_TCP_SACK_OUT==1: TCP will support sending selective acknowledgements (SACKs).
 */
#define LWIP_TCP_SACK_OUT               1



/**
 * LWIP_TCP_RTO_TIME: Initial TCP retransmission timeout (ms).
 * This defaults to 3 seconds as traditionally defined in the TCP protocol.
 * For improving timely recovery on faster networks, this value could
 * be lowered down to 1 second (RFC 6298)
 */
#define LWIP_TCP_RTO_TIME               1000

#endif /* LWIP_HDR_LWIPOPTS_H__ */
