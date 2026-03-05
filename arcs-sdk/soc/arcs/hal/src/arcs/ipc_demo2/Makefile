
CFG_AMP_IPC := 1
WIFI_API := on
CONFIG_PM := 0
CFG_IPC_PRINT := 1

export CFG_AMP_IPC WIFI_API CONFIG_PM CFG_IPC_PRINT

ifeq ("$(strip $(CONFIG_PM))","1")
PSRAM_HEAP = 0
endif
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
