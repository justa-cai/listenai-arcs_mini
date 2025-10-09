#include <stdio.h>
#include <stdint.h>

#include "ClockManager.h"

// 打印关键时钟频率（一次性）
static void print_cmu_summary(void)
{
    uint32_t cpu_hz   = CRM_GetCpuFreq();
    uint32_t hclk_hz  = CRM_GetHclkFreq();
    uint32_t apb_hz   = CRM_GetAp_peri_pclkFreq();
    uint32_t flash_hz = CRM_GetFlashFreq();

    printf("CPU: %u Hz\n", cpu_hz);
    printf("HCLK: %u Hz\n", hclk_hz);
    printf("APB: %u Hz\n", apb_hz);
    printf("FLASH: %u Hz\n", flash_hz);

    // 常用外设频率
    printf("UART0: %u Hz\n", CRM_GetUart0Freq());
    printf("UART1: %u Hz\n", CRM_GetUart1Freq());
    printf("UART2: %u Hz\n", CRM_GetUart2Freq());
    printf("SPI0: %u Hz\n", CRM_GetSpi0Freq());
    printf("I2C0: %u Hz\n", CRM_GetI2c0Freq());
}

// 将时钟源枚举转为字符串
static const char* crm_src_to_str(clock_src_name_t src)
{
    switch (src) {
    case CRM_IpSrcInvalide:      return "Invalide";
    case CRM_IpSrcCoreClk:       return "CoreClk";
    case CRM_IpSrcPsramClk:      return "PsramClk";
    case CRM_IpSrcXtalClk:       return "XtalClk";
    case CRM_IpSrcPeriClk:       return "PeriClk";
    case CRM_IpSrcFlashClk:      return "FlashClk";
    case CRM_IpSrcCmn32kClk:     return "Cmn32k";
    case CRM_IpSrcAon32kClk:     return "Aon32k";
    case CRM_IpSrcBBPLLCoreClk:  return "BBPLLCore";
    default:                     return "Unknown";
    }
}

// 打印关键域/外设的源与分频配置
static void print_cmu_config_usage(void)
{
    clock_src_name_t src = 0;
    uint32_t div_n = 0, div_m = 0;

    // HCLK：源 + 分频 n/m
    HAL_CRM_GetHclkClkConfig(&src, &div_n, &div_m);
    printf("[CMU] HCLK cfg: src=%u(%s) n=%u m=%u\n",
           (unsigned)src, crm_src_to_str(src), div_n, div_m);

    // APB：分频 n/m（源由上游 HCLK 决定）
    HAL_CRM_GetAp_peri_pclkClkConfig(&div_n, &div_m);
    printf("[CMU] APB cfg: n=%u m=%u\n", div_n, div_m);

    // UART0：源 + 分频 n/m
    HAL_CRM_GetUart0ClkConfig(&src, &div_n, &div_m);
    printf("[CMU] UART0 cfg: src=%u(%s) n=%u m=%u\n",
           (unsigned)src, crm_src_to_str(src), div_n, div_m);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    printf("Hello, world! CMU\n");

    print_cmu_summary();


    print_cmu_config_usage();

    printf("CMU check success\n");
    return 0;
}


