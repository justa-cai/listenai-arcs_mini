TOPDIR := ../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB	= liblwip.a
TARGET  =

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS	=

#exclude subdirs
exclude_dirs=

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
LWIPDIR := $(LWIP_VER)
PORT := rtos
CSRCS_CORE := $(LWIPDIR)/src/core/init.c \
              $(LWIPDIR)/src/core/def.c\
              $(LWIPDIR)/src/core/dns.c\
              $(LWIPDIR)/src/core/inet_chksum.c\
              $(LWIPDIR)/src/core/ip.c\
              $(LWIPDIR)/src/core/mem.c\
              $(LWIPDIR)/src/core/memp.c\
              $(LWIPDIR)/src/core/netif.c\
              $(LWIPDIR)/src/core/pbuf.c\
              $(LWIPDIR)/src/core/raw.c\
              $(LWIPDIR)/src/core/stats.c\
              $(LWIPDIR)/src/core/sys.c\
              $(LWIPDIR)/src/core/tcp.c\
              $(LWIPDIR)/src/core/tcp_in.c\
              $(LWIPDIR)/src/core/tcp_out.c\
              $(LWIPDIR)/src/core/timeouts.c\
              $(LWIPDIR)/src/core/udp.c

CSRCS_IPV4 := $(LWIPDIR)/src/core/ipv4/acd.c\
              $(LWIPDIR)/src/core/ipv4/autoip.c\
              $(LWIPDIR)/src/core/ipv4/dhcp.c\
              $(LWIPDIR)/src/core/ipv4/etharp.c\
              $(LWIPDIR)/src/core/ipv4/icmp.c\
              $(LWIPDIR)/src/core/ipv4/igmp.c\
              $(LWIPDIR)/src/core/ipv4/ip4_frag.c\
              $(LWIPDIR)/src/core/ipv4/ip4.c\
              $(LWIPDIR)/src/core/ipv4/ip4_addr.c

CSRCS_NETIF := $(LWIPDIR)/src/netif/ethernet.c

CSRCS_ARCH  := $(LWIPDIR)/contrib/ports/$(PORT)/sys_arch.c

CSRCS_IPV6 := $(LWIPDIR)/src/core/ipv6/dhcp6.c\
              $(LWIPDIR)/src/core/ipv6/ethip6.c\
              $(LWIPDIR)/src/core/ipv6/icmp6.c\
              $(LWIPDIR)/src/core/ipv6/inet6.c\
              $(LWIPDIR)/src/core/ipv6/ip6.c\
              $(LWIPDIR)/src/core/ipv6/ip6_addr.c\
              $(LWIPDIR)/src/core/ipv6/ip6_frag.c\
              $(LWIPDIR)/src/core/ipv6/mld6.c\
              $(LWIPDIR)/src/core/ipv6/nd6.c

CSRCS_API := $(LWIPDIR)/src/api/api_lib.c\
             $(LWIPDIR)/src/api/api_msg.c\
             $(LWIPDIR)/src/api/err.c\
             $(LWIPDIR)/src/api/netbuf.c\
             $(LWIPDIR)/src/api/netdb.c\
             $(LWIPDIR)/src/api/netifapi.c\
             $(LWIPDIR)/src/api/sockets.c\
             $(LWIPDIR)/src/api/tcpip.c

CSRCS_PORT := port/netif/dhcp_state.c

CSRCS_APP := $(LWIPDIR)/src/apps/dhcps/dhcps.c
CSRCS_NET_AL := net_al/net_al.c\
                net_al/net_ip.c\
                net_al/net_tg_al.c
LWIPERFFILES=$(LWIPDIR)/src/apps/lwiperf/lwiperf.c

ifneq ($(CFG_IPERF), "")
CSRCS_APP += net_al/net_iperf.c
CSRCS_NET_AL +=  net_al/net_iperf_al.c
endif
ifneq ($(CFG_PING), "")
CSRCS_APP += net_al/net_ping.c
endif

CPPSRCS	=
SSRCS	=
CSRCS  := $(CSRCS_PORT) $(CSRCS_CORE) $(CSRCS_IPV4) $(CSRCS_NETIF) $(CSRCS_ARCH) $(CSRCS_APP) $(CSRCS_API) $(CSRCS_NET_AL) $(LWIPERFFILES)
 
#includings and flags
CFLAGS := -I$(LWIPDIR)/src/include\
          -I$(LWIPDIR)/src/include/posix\
          -I$(LWIPDIR)/src/include/lwip/apps\
          -I$(LWIPDIR)/contrib/ports/$(PORT)/include\
          -I$(TOPDIR)/include/bsp\
          -I$(TOPDIR)/modules/rtos/include\
          -I$(TOPDIR)/chip/$(CHIP)/rtos/rtos_al\
          -I$(TOPDIR)/modules/rtos/portable/${BUILDTOOL}/non_secure\
          -I$(TOPDIR)/chip/$(CHIP)/include\
          -I$(TOPDIR)/chip/$(CHIP)/include/wifi\
          -I$(TOPDIR)/chip/$(CHIP)/include/net\
          -I$(TOPDIR)/chip/$(CHIP)/include/utils\
          -I$(TOPDIR)/chip/$(CHIP)/driver/debug\
          -I$(TOPDIR)/chip/$(CHIP)/cli_cmd \
          -I$(TOPDIR)/chip/$(CHIP)/lwip/port/include

ifeq ($(TX_BUF_COPY), 1)
CFLAGS += -DTX_BUF_COPY
endif

include $(TOPDIR)/rules.mk
