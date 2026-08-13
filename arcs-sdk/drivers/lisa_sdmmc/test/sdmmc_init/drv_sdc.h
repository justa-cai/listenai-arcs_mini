#ifndef TEST_DRV_SDC_H
#define TEST_DRV_SDC_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t u32;
typedef void (*sdc_platform_setting_t)(void);

#define SD_0 0U

#define SDC_OPTION_ENABLE          0x01U
#define SDC_OPTION_FIXED           0x02U
#define SDC_OPTION_SDIO_STD_FUNC   0x04U
#define SDC_OPTION_SDIO_FORCE_3_3_V 0x08U

#define GM_SDC_ACTION_INIT             1U
#define GM_SDC_ACTION_CARD_SCAN        2U
#define GM_SDC_ACTION_SET_BUS_WIDTH    3U
#define GM_SDC_ACTION_SET_ADMA_BUFER   4U
#define GM_SDC_ACTION_SOFT_RESET       5U
#define GM_SDC_ACTION_CARD_DETECTION   6U
#define GM_SDC_ACTION_IS_CARD_EXIST    7U

#define SDHCI_SOFTRST_ALL 0xFFU

u32 gm_api_sdc_platform_init(u32 sdc0_option,
                             u32 sdc1_option,
                             sdc_platform_setting_t hw_setting,
                             u32 card_buffer);
u32 gm_sdc_api_action(uint8_t ip_idx, u32 type, void *in, void *out);

#endif
