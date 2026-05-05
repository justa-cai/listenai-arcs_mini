#include <stdint.h>
#include <string.h>

#include "chip.h"
#include "Driver_TRNG.h"
#include "trng.h"

#define TRNG_REG_PROTECK_KEY 0xF5000000

#define CSK_TRNG_CONIFG_REG_DELAYTIME_Pos 0
#define CSK_TRNG_CONIFG_REG_HOTTIME_Pos   4
#define CSK_TRNG_CONIFG_REG_COLDTIME_Pos  8

int trng_get(uint8_t *output, uint32_t len, uint32_t *olen)
{
    uint32_t request_len;
    uint32_t control;
    uint32_t reg_value;
    uint32_t tmp;

    if (output == NULL || olen == NULL || len == 0U) {
        return -1;
    }

    request_len = len > 0xFFFFU ? 0xFFFFU : len;

    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_TRNG_CLK = 0x1;

    control = CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11;
    reg_value = TRNG_REG_PROTECK_KEY;

    tmp = (control & CSK_TRNG_DELAYTIME_CONTROL_Msk) >> CSK_TRNG_DELAYTIME_CONTROL_Pos;
    reg_value |= tmp << CSK_TRNG_CONIFG_REG_DELAYTIME_Pos;
    tmp = (control & CSK_TRNG_HOTTIME_CONTROL_Msk) >> CSK_TRNG_HOTTIME_CONTROL_Pos;
    reg_value |= tmp << CSK_TRNG_CONIFG_REG_HOTTIME_Pos;
    tmp = (control & CSK_TRNG_COLDTIME_CONTROL_Msk) >> CSK_TRNG_COLDTIME_CONTROL_Pos;
    reg_value |= tmp << CSK_TRNG_CONIFG_REG_COLDTIME_Pos;

    IP_TRNG->REG_TRNG_CONFIG.all = reg_value;

    for (uint32_t cnt = 0; cnt < request_len;) {
        uint32_t trng_data;
        uint32_t copy_len;

        IP_TRNG->REG_TRNG_CTRL.all = TRNG_REG_PROTECK_KEY | 1U;
        while (IP_TRNG->REG_TRNG_STATUS.bit.DREADY == 0U) {
        }

        trng_data = IP_TRNG->REG_TRNG_DATA.all;
        copy_len = ((cnt + 4U) > request_len) ? (request_len - cnt) : 4U;
        memcpy(output + cnt, &trng_data, copy_len);
        cnt += copy_len;
    }

    *olen = request_len;
    return 0;
}
