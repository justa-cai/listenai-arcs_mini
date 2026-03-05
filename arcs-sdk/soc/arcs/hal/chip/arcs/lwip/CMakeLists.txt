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

    # Get LWIP version from Kconfig
    set(LWIP_VERSION_DIR "lwip-${CONFIG_LWIP_VERSION}")

    # Core files
    set(LWIP_CORE_SRCS
        ${LWIP_VERSION_DIR}/src/core/init.c
        ${LWIP_VERSION_DIR}/src/core/def.c
        ${LWIP_VERSION_DIR}/src/core/dns.c
        ${LWIP_VERSION_DIR}/src/core/inet_chksum.c
        ${LWIP_VERSION_DIR}/src/core/ip.c
        ${LWIP_VERSION_DIR}/src/core/mem.c
        ${LWIP_VERSION_DIR}/src/core/memp.c
        ${LWIP_VERSION_DIR}/src/core/netif.c
        ${LWIP_VERSION_DIR}/src/core/pbuf.c
        ${LWIP_VERSION_DIR}/src/core/raw.c
        ${LWIP_VERSION_DIR}/src/core/stats.c
        ${LWIP_VERSION_DIR}/src/core/sys.c
        ${LWIP_VERSION_DIR}/src/core/tcp.c
        ${LWIP_VERSION_DIR}/src/core/tcp_in.c
        ${LWIP_VERSION_DIR}/src/core/tcp_out.c
        ${LWIP_VERSION_DIR}/src/core/timeouts.c
        ${LWIP_VERSION_DIR}/src/core/udp.c
    )

    # IPv4 files
    set(LWIP_IPV4_SRCS
        ${LWIP_VERSION_DIR}/src/core/ipv4/acd.c
        ${LWIP_VERSION_DIR}/src/core/ipv4/autoip.c
        ${LWIP_VERSION_DIR}/src/core/ipv4/dhcp.c
        ${LWIP_VERSION_DIR}/src/core/ipv4/etharp.c
        ${LWIP_VERSION_DIR}/src/core/ipv4/icmp.c
        ${LWIP_VERSION_DIR}/src/core/ipv4/igmp.c
        ${LWIP_VERSION_DIR}/src/core/ipv4/ip4_frag.c
        ${LWIP_VERSION_DIR}/src/core/ipv4/ip4.c
        ${LWIP_VERSION_DIR}/src/core/ipv4/ip4_addr.c
    )


    set(LWIP_NET_SRCS
        net_al/net_al.c
        net_al/net_ip.c
        net_al/net_tg_al.c
        ${LWIP_VERSION_DIR}/contrib/ports/rtos/sys_arch.c
    )

    if (DEFINED CONFIG_LWIP_DHCP_RESTORE_LAST_IP_FROM_LISA_KV)
        list(APPEND LWIP_NET_SRCS port/netif/dhcp_state_kv.c)
    else()
        list(APPEND LWIP_NET_SRCS port/netif/dhcp_state.c)
    endif()

    # IPv6 files
    set(LWIP_IPV6_SRCS
        ${LWIP_VERSION_DIR}/src/core/ipv6/dhcp6.c
        ${LWIP_VERSION_DIR}/src/core/ipv6/ethip6.c
        ${LWIP_VERSION_DIR}/src/core/ipv6/icmp6.c
        ${LWIP_VERSION_DIR}/src/core/ipv6/inet6.c
        ${LWIP_VERSION_DIR}/src/core/ipv6/ip6.c
        ${LWIP_VERSION_DIR}/src/core/ipv6/ip6_addr.c
        ${LWIP_VERSION_DIR}/src/core/ipv6/ip6_frag.c
        ${LWIP_VERSION_DIR}/src/core/ipv6/mld6.c
        ${LWIP_VERSION_DIR}/src/core/ipv6/nd6.c
    )

    # API files
    set(LWIP_API_SRCS
        ${LWIP_VERSION_DIR}/src/api/api_lib.c
        ${LWIP_VERSION_DIR}/src/api/api_msg.c
        ${LWIP_VERSION_DIR}/src/api/err.c
        ${LWIP_VERSION_DIR}/src/api/netbuf.c
        ${LWIP_VERSION_DIR}/src/api/netdb.c
        ${LWIP_VERSION_DIR}/src/api/netifapi.c
        ${LWIP_VERSION_DIR}/src/api/sockets.c
        ${LWIP_VERSION_DIR}/src/api/tcpip.c
    )

    # APP files
    set(LWIP_APP_SRCS
        ${LWIP_VERSION_DIR}/src/apps/dhcps/dhcps.c
        ${LWIP_VERSION_DIR}/src/apps/sntp/sntp.c
    )
    set(LWIPERF_SRCS  ${LWIP_VERSION_DIR}/src/apps/lwiperf/lwiperf.c)

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
        ${LWIP_VERSION_DIR}/src/netif/ethernet.c
    )

    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/net
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/port/include
        ${LWIP_VERSION_DIR}/src/include
        ${LWIP_VERSION_DIR}/contrib/ports/rtos/include
        ${LWIP_VERSION_DIR}/src/include/lwip/apps
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

