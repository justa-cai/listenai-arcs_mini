# LISA WiFi
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_WIFI)

listenai_library_named(lisa_wifi)

listenai_library_sources(
    lisa_wifi.c
)

# 双核模式下C核/单核模式下需要依赖该文件
if((CONFIG_WIFI_LWIP_DIFF_CORE AND CONFIG_LWIP) OR CONFIG_WIFI_LWIP_SAME_CORE)
    listenai_library_sources(
        lisa_wifi_adapter.c
    )
endif()

listenai_include_directories(.)

endif()
