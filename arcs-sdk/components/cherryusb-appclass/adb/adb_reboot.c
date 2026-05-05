#define LOG_TAG "adb.reboot"

#include "adb_services.h"
#include "adb.h"
#include "adb_utils.h"

#include <stdint.h>
#include <string.h>

#include "arcs_ap.h"
#include "chip.h"

static void do_reboot(void)
{
	extern void sys_platform_sw_full_reset(void);
	sys_platform_sw_full_reset();
}

static void do_recovery(void)
{
	struct boot_info {
		uint32_t reboot_cnt: 8;
		uint32_t recover_reason: 8;
		uint32_t reserved: 15;
		uint32_t req: 1;
	};

	uint32_t rstCause;
	rstCause = IP_AON_CTRL->REG_SYSRST_STATUS.all;
	IP_AON_CTRL->REG_SYSRST_STATUS.all = rstCause;

	struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
	info->req = 1;
	info->reboot_cnt = 0;

	IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all &= ~(0b1111 << 21);
	IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all |= (0b1110 << 21);

	IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CMN_RST_EN = 1;
	IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CP_RST_EN = 1;
	IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2AP_RST_EN = 1;
	__COMPILER_BARRIER();
	IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
}

static int adb_reboot_open(struct adb_service *s, const uint8_t *args)
{
	if (args == NULL || args[0] == '\0') {
		ADB_LOGI("adb reboot\n");
		do_reboot();
	} else if (strcmp((const char *)args, "recovery") == 0) {
		ADB_LOGI("adb reboot recovery\n");
		do_recovery();
	} else {
		ADB_LOGE("adb reboot: unknown arg: %s\n", args);
		return -1;
	}

	/* should not reach here */
	return 0;
}

static int adb_reboot_close(struct adb_service *s)
{
	return 0;
}

static int adb_reboot_write(struct adb_service *s, adb_packet_t *p)
{
	adb_packet_free(p);
	return 0;
}

static const struct adb_service_handle adb_reboot_handle = {
	.name = "reboot",
	.open = adb_reboot_open,
	.close = adb_reboot_close,
	.write = adb_reboot_write,
};

void adb_reboot_init(void)
{
	adb_service_hd_register(&adb_reboot_handle);
}
