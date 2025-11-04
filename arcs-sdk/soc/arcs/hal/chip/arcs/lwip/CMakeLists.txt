if (DEFINED CONFIG_LWIP)
    listenai_library_named(psram_lwip)

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )

    if (CONFIG_LWIP_TX_BUF_COPY)
        listenai_compile_definitions(TX_BUF_COPY=1)
    endif() # CONFIG_LWIP_TX_BUF_COPY

    # Add LWIP configurations
    if (DEFINED CONFIG_IPERF)
        target_compile_definitions(${LISTENAI_CURRENT_LIBRARY} PRIVATE CFG_IPERF)
    endif()

    if (DEFINED CONFIG_PING)
        target_compile_definitions(${LISTENAI_CURRENT_LIBRARY} PRIVATE CFG_PING)
    endif()



    # Core files
    set(LWIP_CORE_SRCS
        lwip-2.2.0/src/core/init.c
        lwip-2.2.0/src/core/def.c
        lwip-2.2.0/src/core/dns.c
        lwip-2.2.0/src/core/inet_chksum.c
        lwip-2.2.0/src/core/ip.c
        lwip-2.2.0/src/core/mem.c
        lwip-2.2.0/src/core/memp.c
        lwip-2.2.0/src/core/netif.c
        lwip-2.2.0/src/core/pbuf.c
        lwip-2.2.0/src/core/raw.c
        lwip-2.2.0/src/core/stats.c
        lwip-2.2.0/src/core/sys.c
        lwip-2.2.0/src/core/tcp.c
        lwip-2.2.0/src/core/tcp_in.c
        lwip-2.2.0/src/core/tcp_out.c
        lwip-2.2.0/src/core/timeouts.c
        lwip-2.2.0/src/core/udp.c
    )

    # IPv4 files
    set(LWIP_IPV4_SRCS
        lwip-2.2.0/src/core/ipv4/acd.c
        lwip-2.2.0/src/core/ipv4/autoip.c
        lwip-2.2.0/src/core/ipv4/dhcp.c
        lwip-2.2.0/src/core/ipv4/etharp.c
        lwip-2.2.0/src/core/ipv4/icmp.c
        lwip-2.2.0/src/core/ipv4/igmp.c
        lwip-2.2.0/src/core/ipv4/ip4_frag.c
        lwip-2.2.0/src/core/ipv4/ip4.c
        lwip-2.2.0/src/core/ipv4/ip4_addr.c
    )


    set(LWIP_NET_SRCS
        net_al/net_al.c
        net_al/net_ip.c
        net_al/net_tg_al.c
        lwip-2.2.0/contrib/ports/rtos/sys_arch.c
        port/netif/dhcp_state.c
    )

    # IPv6 files
    set(LWIP_IPV6_SRCS
        lwip-2.2.0/src/core/ipv6/dhcp6.c
        lwip-2.2.0/src/core/ipv6/ethip6.c
        lwip-2.2.0/src/core/ipv6/icmp6.c
        lwip-2.2.0/src/core/ipv6/inet6.c
        lwip-2.2.0/src/core/ipv6/ip6.c
        lwip-2.2.0/src/core/ipv6/ip6_addr.c
        lwip-2.2.0/src/core/ipv6/ip6_frag.c
        lwip-2.2.0/src/core/ipv6/mld6.c
        lwip-2.2.0/src/core/ipv6/nd6.c
    )

    # API files
    set(LWIP_API_SRCS
        lwip-2.2.0/src/api/api_lib.c
        lwip-2.2.0/src/api/api_msg.c
        lwip-2.2.0/src/api/err.c
        lwip-2.2.0/src/api/netbuf.c
        lwip-2.2.0/src/api/netdb.c
        lwip-2.2.0/src/api/netifapi.c
        lwip-2.2.0/src/api/sockets.c
        lwip-2.2.0/src/api/tcpip.c
    )

    # APP files
    set(LWIP_APP_SRCS
        lwip-2.2.0/src/apps/dhcps/dhcps.c
        lwip-2.2.0/src/apps/sntp/sntp.c
    )
    set(LWIPERF_SRCS  lwip-2.2.0/src/apps/lwiperf/lwiperf.c)

    if (DEFINED CONFIG_IPERF)
        list(APPEND LWIP_APP_SRCS
            net_al/net_iperf.c
            net_al/net_iperf_al.c
        )
    endif()

    if (DEFINED CONFIG_PING)
        list(APPEND LWIP_APP_SRCS
            net_al/net_ping.c
        )
    endif()

    listenai_library_sources(
        ${LWIP_CORE_SRCS}
        ${LWIP_IPV4_SRCS}
        ${LWIP_IPV6_SRCS}
        ${LWIP_API_SRCS}
        ${LWIP_NET_SRCS}
        ${LWIP_APP_SRCS}
        ${LWIPERF_SRCS}
        lwip-2.2.0/src/netif/ethernet.c
    )

    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/net
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/port/include
        lwip-2.2.0/src/include
        lwip-2.2.0/contrib/ports/rtos/include
        lwip-2.2.0/src/include/lwip/apps
        net_al
    )

    # options
    if (DEFINED CONFIG_LWIP_OPTION_LWIP_DNS)
        listenai_compile_definitions(LWIP_DNS=1)
    endif()

    if (DEFINED CONFIG_LWIP_OPTION_SO_REUSE)
        listenai_compile_definitions(SO_REUSE=1)
    endif()

endif()

