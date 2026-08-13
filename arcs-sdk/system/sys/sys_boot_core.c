#include <stdint.h>

#include "soc/chip.h"
#include "sys/boot_core.h"

int sys_boot_core(uint8_t targe_core_id, uint32_t boot_addr)
{
    return soc_boot_core(targe_core_id, boot_addr);
}
