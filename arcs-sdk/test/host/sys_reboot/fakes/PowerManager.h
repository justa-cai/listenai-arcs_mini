#ifndef TEST_HOST_SYS_REBOOT_POWERMANAGER_H_
#define TEST_HOST_SYS_REBOOT_POWERMANAGER_H_

void fake_soft_reset(void);

#define __HAL_PMU_WholeChip_RST_ENABLE() fake_soft_reset()

#endif
