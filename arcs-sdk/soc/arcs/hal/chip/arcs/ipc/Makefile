TOPDIR = ../../..

#lib or target you want to build: eg. LIB=libmisc.a  or TARGET=wlan
LIB    = libipc.a
TARGET =

#libraries depend: eg. LIBS = -lbsp -ldrv -lrtos
LIBS   =

#exclude subdirs
exclude_dirs=

#includings and flags
CFLAGS := -I$(TOPDIR)/include/rtos \
          -I$(TOPDIR)/include/debug \
          -I$(TOPDIR)/include/driver \
          -I$(TOPDIR)/chip/${CHIP}/include \
          -I$(TOPDIR)/chip/${CHIP}/include/wifi \
          -I$(TOPDIR)/chip/${CHIP}/include/bt \
          -I$(TOPDIR)/chip/${CHIP}/include/ipc \
          -I$(TOPDIR)/chip/${CHIP}/include/ipc/mrpc \
          -I$(TOPDIR)/chip/${CHIP}/include/utils \
          -I$(TOPDIR)/chip/$(CHIP)/include/net \
          -I$(TOPDIR)/chip/$(CHIP)/btos/task/api \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/include \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/include/mrpc \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/service \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/service/ipc_bus \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/service/wifi/mrpc \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/service/bt/mrpc \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/service/bt_s2m/mrpc \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/service/lwip/mrpc \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/service/nvs/mrpc \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/service/flash_if/mrpc \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/service/luna/mrpc \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/service/utils_s2m/mrpc \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/service/utils_m2s/mrpc \
          -I$(TOPDIR)/chip/${CHIP}/driver/private_include \
          -I$(TOPDIR)/chip/${CHIP}/driver/spiflash \
          -I$(TOPDIR)/chip/${CHIP}/rtos/rtos_al \
          -I$(TOPDIR)/chip/$(CHIP)/lwip/port/include \
          -I$(TOPDIR)/modules/rtos/portable/riscv_nuclei/non_secure\
          -I$(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include\
          -I$(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/posix\
          -I$(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/compat/stdc\
          -I$(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/src/include/lwip/apps\
          -I$(TOPDIR)/chip/$(CHIP)/lwip/$(LWIP_VER)/contrib/ports/rtos/include\
          -I$(TOPDIR)/chip/${CHIP}/ic_lock\
          -I$(TOPDIR)/chip/${CHIP}/btos

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS	   := utils/ipc_utils.c utils/amp_shared.c
CSRCS_CORE := core/ipc_core.c core/ipc_msg.c core/ipc_mem.c core/ipc_ep.c core/ipc_platform.c core/ipc_queue.c core/ipc_rx_task.c
CSRCS_SERVICE := service/ipc_mrpc_service.c
CSRCS_MASTER  :=
CSRCS_SLAVE   :=

ifneq ("$(filter 1,$(strip $(CFG_AMP_IPC_BUS) $(CFG_AMP_IPC_BUS_DEMO)))", "")
CSRCS_SERVICE += service/ipc_bus/ipc_bus.c
endif
ifeq ("$(strip $(CFG_AMP_IPC_BUS_DEMO))", "1")
CSRCS_SERVICE += examples/ipc_bus/ipc_bus_demo.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MASTER))", "1")
CSRCS_MASTER += role/ipc_master.c service/event/ipc_event_fwd.c
ifeq ("$(strip $(CFG_AMP_IPC_WIFI_CHAN))", "1")
CSRCS_MASTER += service/wifi/ipc_master_wifi.c
endif
ifeq ("$(strip $(CFG_AMP_IPC_BT_CHAN))", "1")
CSRCS_MASTER += service/bt/ipc_master_bt.c
endif
ifeq ("$(strip $(CFG_IPC_TEST_CASE))", "1")
CSRCS_MASTER += mrpc/test/master2slave/mrpc_m2s_test_api_client.c mrpc/test/slave2master/mrpc_s2m_test_api_server.c test/ipc_test.c
endif
ifeq ("$(strip $(CFG_IPC_PRINT))", "1")
CSRCS_MASTER += debug/ipc_dbg_reader.c
endif
endif
ifeq ("$(strip $(CFG_AMP_IPC_SLAVE))", "1")
CSRCS_SLAVE  += role/ipc_slave.c service/event/ipc_event_fwd.c
ifeq ("$(strip $(CFG_AMP_IPC_WIFI_CHAN))", "1")
CSRCS_SLAVE += service/wifi/ipc_slave_wifi.c
endif
ifeq ("$(strip $(CFG_AMP_IPC_BT_CHAN))", "1")
CSRCS_SLAVE += service/bt/ipc_slave_bt.c
endif
ifeq ("$(strip $(CFG_IPC_TEST_CASE))", "1")
CSRCS_SLAVE  += mrpc/test/master2slave/mrpc_m2s_test_api_server.c mrpc/test/slave2master/mrpc_s2m_test_api_client.c test/ipc_test.c
endif
ifeq ("$(strip $(CFG_IPC_PRINT))", "1")
CSRCS_SLAVE  += debug/ipc_dbg_writer.c
endif
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_SERVER))", "1")
CSRCS_MRPC += mrpc/mrpc_server.c
endif
ifeq ("$(strip $(CFG_AMP_IPC_MRPC_CLIENT))", "1")
CSRCS_MRPC += mrpc/mrpc_client.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_SERVER_LWIP))", "1")
CSRCS_MRPC += service/lwip/mrpc/mrpc_lwip_api_server.c
endif
ifeq ("$(strip $(CFG_AMP_IPC_MRPC_SERVER_BT))", "1")
CSRCS_MRPC += service/bt/mrpc/mrpc_bt_api_server.c
endif
ifeq ("$(strip $(CFG_AMP_IPC_MRPC_SERVER_BT_S2M))", "1")
CSRCS_MRPC += service/bt_s2m/mrpc/mrpc_bt_s2m_api_server.c
endif
ifeq ("$(strip $(CFG_AMP_IPC_MRPC_CLIENT_LWIP))", "1")
CSRCS_MRPC += service/lwip/mrpc/mrpc_lwip_api_client.c
endif
ifeq ("$(strip $(CFG_AMP_IPC_MRPC_CLIENT_BT))", "1")
CSRCS_MRPC += service/bt/mrpc/mrpc_bt_api_client.c
endif
ifeq ("$(strip $(CFG_AMP_IPC_MRPC_CLIENT_BT_S2M))", "1")
CSRCS_MRPC += service/bt_s2m/mrpc/mrpc_bt_s2m_api_client.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_SERVER_WIFI))", "1")
CSRCS_MRPC += service/wifi/mrpc/mrpc_wifi_api_server.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_CLIENT_WIFI))", "1")
CSRCS_MRPC += service/wifi/mrpc/mrpc_wifi_api_client.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_CLIENT_NVS))", "1")
CSRCS_MRPC += service/nvs/mrpc/mrpc_nvs_api_client.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_SERVER_NVS))", "1")
CSRCS_MRPC += service/nvs/mrpc/mrpc_nvs_api_server.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_CLIENT_FLASH_IF))", "1")
CSRCS_MRPC += service/flash_if/mrpc/mrpc_flash_if_api_client.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_SERVER_FLASH_IF))", "1")
CSRCS_MRPC += service/flash_if/mrpc/mrpc_flash_if_api_server.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_CLIENT_UTILS_S2M))", "1")
CSRCS_MRPC += service/utils_s2m/mrpc/mrpc_utils_s2m_api_client.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_SERVER_UTILS_S2M))", "1")
CSRCS_MRPC += service/utils_s2m/mrpc/mrpc_utils_s2m_api_server.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_CLIENT_UTILS_M2S))", "1")
CSRCS_MRPC += service/utils_m2s/mrpc/mrpc_utils_m2s_api_client.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_SERVER_UTILS_M2S))", "1")
CSRCS_MRPC += service/utils_m2s/mrpc/mrpc_utils_m2s_api_server.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_CLIENT_LUNA))", "1")
CSRCS_MRPC += service/luna/mrpc/mrpc_luna_api_client.c
endif

ifeq ("$(strip $(CFG_AMP_IPC_MRPC_SERVER_LUNA))", "1")
CSRCS_MRPC += service/luna/mrpc/mrpc_luna_api_server.c
endif

CSRCS	+= $(CSRCS_CORE) $(CSRCS_SERVICE) $(CSRCS_MASTER) $(CSRCS_SLAVE) $(CSRCS_MRPC)
CPPSRCS	=
SSRCS	=



ifeq ("$(strip $(CFG_IPC_TEST_CASE))", "1")
CFLAGS += -DIPC_TEST_CASE
CFLAGS += -I$(TOPDIR)/chip/${CHIP}/ipc/mrpc/test/master2slave \
          -I$(TOPDIR)/chip/${CHIP}/ipc/mrpc/test/slave2master
endif

#ifeq ("$(CFG_AMP_IPC_MASTER)", "")
ifeq ("$(strip $(CFG_AMP_IPC_INDICATION))", "1")
CFLAGS += -DCFG_AMP_IPC_INDICATION
endif

pre:
	@#python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py wifi service/wifi/mrpc mrpc/template include/mrpc
	@#python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py lwip service/lwip/mrpc mrpc/template include/mrpc
	@#python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py bt service/bt/mrpc mrpc/template include/mrpc
	@#python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py bt_s2m service/bt_s2m/mrpc mrpc/template include/mrpc
	@#python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py nvs service/nvs/mrpc mrpc/template include/mrpc
	@#python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py flash_if service/flash_if/mrpc mrpc/template include/mrpc
	@#python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py utils_s2m service/utils_s2m/mrpc mrpc/template include/mrpc
	@#python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py utils_m2s service/utils_m2s/mrpc mrpc/template include/mrpc
	@#python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py luna service/luna/mrpc mrpc/template include/mrpc
ifeq ("$(strip $(CFG_IPC_TEST_CASE))", "1")
	@#python $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py m2s_test mrpc/test/master2slave mrpc/test/master2slave/template include/mrpc
	@#python $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py s2m_test mrpc/test/slave2master mrpc/test/slave2master/template include/mrpc
endif
#endif
include $(TOPDIR)/rules.mk
