#include "unity.h"

#ifdef RUN_TEST
#undef RUN_TEST
#endif
#define RUN_TEST(TestFunc, TestLineNum) UnityDefaultTestRun(TestFunc, #TestFunc, TestLineNum)

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "arcs_ap.h"
#include "drv_sdc.h"
#include "FreeRTOS.h"
#include "lib_sdc.h"

int sdmmc_hard_init(void);
int sdmmc_platform_init(void);

test_ap_cfg_t g_test_ap_cfg;
test_sdioh_t g_test_sdioh;

static uint32_t g_card_detection_calls;
static uint32_t g_adma_calls;
static uint32_t g_bus_width_calls;
static uint32_t g_platform_option;
static u32 g_card_detection_result;

void HAL_CRM_SetSdio_hClkDiv(uint32_t n, uint32_t m)
{
    (void)n;
    (void)m;
}

void HAL_CRM_SetSdio_hClkSrc(uint32_t src)
{
    (void)src;
}

TickType_t xTaskGetTickCount(void)
{
    return 0;
}

void vTaskDelay(TickType_t ticks)
{
    (void)ticks;
}

u32 gm_api_sdc_platform_init(u32 sdc0_option,
                             u32 sdc1_option,
                             sdc_platform_setting_t hw_setting,
                             u32 card_buffer)
{
    (void)sdc1_option;
    (void)card_buffer;

    g_platform_option = sdc0_option;
    if (hw_setting != NULL) {
        hw_setting();
    }

    return ERR_SD_NO_ERROR;
}

u32 gm_sdc_api_action(uint8_t ip_idx, u32 type, void *in, void *out)
{
    (void)ip_idx;
    (void)in;
    (void)out;

    switch (type) {
    case GM_SDC_ACTION_SET_ADMA_BUFER:
        g_adma_calls++;
        return ERR_SD_NO_ERROR;
    case GM_SDC_ACTION_CARD_DETECTION:
        g_card_detection_calls++;
        return g_card_detection_result;
    case GM_SDC_ACTION_SET_BUS_WIDTH:
        g_bus_width_calls++;
        return ERR_SD_NO_ERROR;
    default:
        return ERR_SD_OTHER_ERROR;
    }
}

void setUp(void)
{
    memset(&g_test_ap_cfg, 0, sizeof(g_test_ap_cfg));
    memset(&g_test_sdioh, 0, sizeof(g_test_sdioh));
    g_card_detection_calls = 0;
    g_adma_calls = 0;
    g_bus_width_calls = 0;
    g_platform_option = 0;
    g_card_detection_result = 0;
}

void tearDown(void)
{
}

void test_platform_keeps_legacy_fixed_card_sequence(void)
{
    TEST_ASSERT_EQUAL_UINT32(ERR_SD_NO_ERROR, sdmmc_platform_init());
    TEST_ASSERT_TRUE((g_platform_option & SDC_OPTION_ENABLE) != 0U);
    TEST_ASSERT_TRUE((g_platform_option & SDC_OPTION_SDIO_STD_FUNC) != 0U);
    TEST_ASSERT_TRUE((g_platform_option & SDC_OPTION_SDIO_FORCE_3_3_V) != 0U);
    TEST_ASSERT_TRUE((g_platform_option & SDC_OPTION_FIXED) != 0U);
    TEST_ASSERT_EQUAL_UINT32(1, g_adma_calls);
}

void test_hard_init_uses_legacy_card_detection(void)
{
    g_card_detection_result = 0;

    TEST_ASSERT_EQUAL_INT(0, sdmmc_hard_init());
    TEST_ASSERT_EQUAL_UINT32(1, g_card_detection_calls);
    TEST_ASSERT_EQUAL_UINT32(1, g_bus_width_calls);
}

void test_hard_init_fails_when_card_detection_fails(void)
{
    g_card_detection_result = (u32)-EIO;

    TEST_ASSERT_EQUAL_INT(-EIO, sdmmc_hard_init());
    TEST_ASSERT_EQUAL_UINT32(1, g_card_detection_calls);
    TEST_ASSERT_EQUAL_UINT32(0, g_bus_width_calls);
}

int main(void)
{
    UnityBegin("drivers/lisa_sdmmc/test/sdmmc_init/test_sdmmc_init.c");

    RUN_TEST(test_platform_keeps_legacy_fixed_card_sequence, __LINE__);
    RUN_TEST(test_hard_init_uses_legacy_card_detection, __LINE__);
    RUN_TEST(test_hard_init_fails_when_card_detection_fails, __LINE__);

    return UnityEnd();
}
