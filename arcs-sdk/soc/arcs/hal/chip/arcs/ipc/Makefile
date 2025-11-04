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
          -I$(TOPDIR)/chip/${CHIP}/include/ipc \
          -I$(TOPDIR)/chip/${CHIP}/include/utils \
          -I$(TOPDIR)/chip/$(CHIP)/include/net \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/ipc_core \
          -I$(TOPDIR)/chip/$(CHIP)/ipc/ipc_utils \
          -I$(TOPDIR)/chip/${CHIP}/ipc/mrpc/wifi \
          -I$(TOPDIR)/chip/${CHIP}/ipc/mrpc/lwip \
          -I$(TOPDIR)/chip/${CHIP}/ipc/mrpc/nvs \
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
          -I$(TOPDIR)/chip/${CHIP}/ic_lock

#files want to build: eg. CSRCS=a.c  CPPSRCS=b.cpp  SSRCS=c.S
CSRCS_CORE := ipc_core/ipc_core.c ipc_core/ipc_msg.c ipc_core/ipc_mem.c ipc_core/ipc_ep.c ipc_core/ipc_platform.c ipc_core/ipc_queue.c
CSRCS_MASTER  :=
CSRCS_SLAVE   :=
ifeq ("$(CFG_AMP_IPC_MASTER)", "1")
CSRCS_MASTER += ipc_master/ipc_master.c ipc_master/ipc_master_utils.c ipc_utils/ipc_utils.c
ifneq ("$(CFG_IPC_TEST_CASE)", "")
CSRCS_MASTER += mrpc/ipc_master2slave_test/mrpc_m2s_test_api_client.c mrpc/ipc_slave2master_test/mrpc_s2m_test_api_server.c ipc_core/ipc_test.c
endif
ifneq ("$(CFG_IPC_PRINT)", "")
CSRCS_MASTER += ipc_master/ipc_print.c
endif
else
CSRCS_SLAVE  += ipc_slave/ipc_slave.c ipc_utils/ipc_utils.c ipc_slave/ipc_event_fwd.c
ifneq ("$(CFG_IPC_TEST_CASE)", "")
CSRCS_SLAVE  += mrpc/ipc_master2slave_test/mrpc_m2s_test_api_server.c mrpc/ipc_slave2master_test/mrpc_s2m_test_api_client.c ipc_core/ipc_test.c
endif
ifneq ("$(CFG_IPC_PRINT)", "")
CSRCS_SLAVE  += ipc_slave/ipc_print.c
endif
endif

ifneq ("$(CFG_AMP_IPC_MRPC_SERVER)", "")
CSRCS_MRPC += mrpc/mrpc_server.c
endif
ifneq ("$(CFG_AMP_IPC_MRPC_CLIENT)", "")
CSRCS_MRPC += mrpc/mrpc_client.c
endif

ifneq ("$(CFG_AMP_IPC_MRPC_SERVER_LWIP)", "")
CSRCS_MRPC += mrpc/lwip/mrpc_lwip_api_server.c
endif

ifneq ("$(CFG_AMP_IPC_MRPC_CLIENT_LWIP)", "")
CSRCS_MRPC += mrpc/lwip/mrpc_lwip_api_client.c
endif

ifneq ("$(CFG_AMP_IPC_MRPC_SERVER_WIFI)", "")
CSRCS_MRPC += mrpc/wifi/mrpc_wifi_api_server.c
endif

ifneq ("$(CFG_AMP_IPC_MRPC_CLIENT_WIFI)", "")
CSRCS_MRPC += mrpc/wifi/mrpc_wifi_api_client.c
endif

ifneq ("$(CFG_AMP_IPC_MRPC_CLIENT_NVS)", "")
CSRCS_MRPC += mrpc/nvs/mrpc_nvs_api_client.c
endif

ifneq ("$(CFG_AMP_IPC_MRPC_SERVER_NVS)", "")
CSRCS_MRPC += mrpc/nvs/mrpc_nvs_api_server.c
endif

ifeq ("$(CFG_AMP_IPC_MRPC_CLIENT_FLASH_IF)", "1")
CSRCS_MRPC += mrpc/flash_if/mrpc_flash_if_api_client.c
endif

ifeq ("$(CFG_AMP_IPC_MRPC_SERVER_FLASH_IF)", "1")
CSRCS_MRPC += mrpc/flash_if/mrpc_flash_if_api_server.c
endif

ifeq ("$(CFG_AMP_IPC_MRPC_CLIENT_UTILS)", "1")
CSRCS_MRPC += mrpc/utils/mrpc_utils_api_client.c
endif

ifeq ("$(CFG_AMP_IPC_MRPC_SERVER_UTILS)", "1")
CSRCS_MRPC += mrpc/utils/mrpc_utils_api_server.c
endif

ifeq ("$(CFG_AMP_IPC_MRPC_CLIENT_OTP)", "1")
CSRCS_MRPC += mrpc/otp/mrpc_otp_api_client.c
endif

ifeq ("$(CFG_AMP_IPC_MRPC_SERVER_OTP)", "1")
CSRCS_MRPC += mrpc/otp/mrpc_otp_api_server.c
endif

CSRCS	:= $(CSRCS_CORE) $(CSRCS_MASTER) $(CSRCS_SLAVE) $(CSRCS_MRPC)
CPPSRCS	=
SSRCS	=



ifneq ("$(CFG_IPC_TEST_CASE)", "")
CFLAGS += -DIPC_TEST_CASE
CFLAGS += -I$(TOPDIR)/chip/${CHIP}/ipc/mrpc/ipc_master2slave_test \
          -I$(TOPDIR)/chip/${CHIP}/ipc/mrpc/ipc_slave2master_test
endif

#ifeq ("$(CFG_AMP_IPC_MASTER)", "")
pre:
	python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py wifi mrpc/wifi mrpc/template ../include/ipc
	python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py lwip mrpc/lwip mrpc/template ../include/ipc
	python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py nvs mrpc/nvs mrpc/template ../include/ipc
	python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py flash_if mrpc/flash_if mrpc/template ../include/ipc
	python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py utils mrpc/utils mrpc/template ../include/ipc
	python3 $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py otp mrpc/otp mrpc/template ../include/ipc

ifneq ("$(CFG_IPC_TEST_CASE)", "")
	python $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py m2s_test mrpc/ipc_master2slave_test mrpc/ipc_master2slave_test/template ../include/ipc
	python $(TOPDIR)/tools/scripts/mrpcgen/mrpc_generate.py s2m_test mrpc/ipc_slave2master_test mrpc/ipc_slave2master_test/template ../include/ipc
endif
#endif
include $(TOPDIR)/rules.mk
