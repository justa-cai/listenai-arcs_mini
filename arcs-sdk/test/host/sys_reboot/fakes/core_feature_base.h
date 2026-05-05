#ifndef TEST_HOST_SYS_REBOOT_CORE_FEATURE_BASE_H_
#define TEST_HOST_SYS_REBOOT_CORE_FEATURE_BASE_H_

void fake_disable_irq(void);
void fake_wfi(void);

#define __disable_irq() fake_disable_irq()
#define __WFI() fake_wfi()

#endif
