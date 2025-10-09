
CFG_AMP_IPC := 1
LWIP_VER := lwip-2.2.0
WIFI_API := on

export CFG_AMP_IPC LWIP_VER WIFI_API

PSRAM_HEAP ?= 1
export  PSRAM_HEAP

TX_BUF_COPY ?= 0
export TX_BUF_COPY

.PHONY: apps ap_tgt cp_tgt

apps: ap_tgt cp_tgt


ap_tgt:
	$(MAKE) -C ap apps CONFIG_HARTID=0


cp_tgt:
	$(MAKE) -C cp apps CONFIG_HARTID=1

clean:
	$(MAKE) -C ap clean
	$(MAKE) -C cp clean
