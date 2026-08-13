#ifndef TEST_ARCS_AP_H
#define TEST_ARCS_AP_H

#include <stdint.h>

typedef struct {
    struct {
        uint32_t ENA_SDIOH_CLK;
    } bit;
} test_clk_cfg1_reg_t;

typedef struct {
    test_clk_cfg1_reg_t REG_CLK_CFG1;
} test_ap_cfg_t;

typedef struct {
    struct {
        uint32_t LO_SD_RSTN;
    } bit;
} test_sdioh_vr1_reg_t;

typedef struct {
    struct {
        uint32_t UPPER_BIT_SD_CLK_SEL;
        uint32_t LOW_BIT_SD_CLK_SEL;
        uint32_t SD_CLK_EN;
    } bit;
} test_sdioh_ccr_tcr_srr_reg_t;

typedef struct {
    struct {
        uint32_t SD_BUS_POW;
        uint32_t SD_BUS_VOL;
        uint32_t CD_TEST_LV;
        uint32_t CD_SEL;
    } bit;
} test_sdioh_hc1_pcr_bgcr_reg_t;

typedef struct {
    test_sdioh_vr1_reg_t REG_VR1;
    test_sdioh_ccr_tcr_srr_reg_t REG_CCR_TCR_SRR;
    test_sdioh_hc1_pcr_bgcr_reg_t REG_HC1_PCR_BGCR;
} test_sdioh_t;

extern test_ap_cfg_t g_test_ap_cfg;
extern test_sdioh_t g_test_sdioh;

#define IP_AP_CFG (&g_test_ap_cfg)
#define IP_SDIOH  (&g_test_sdioh)

#endif
