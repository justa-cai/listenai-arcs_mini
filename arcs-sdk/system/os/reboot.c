#include "sys/reboot.h"

#include "core_feature_base.h"

void sys_arch_reboot(int type);

void sys_reboot(int type)
{
    __disable_irq();
    sys_arch_reboot(type);

    for (;;) {
        __WFI();
    }
}
