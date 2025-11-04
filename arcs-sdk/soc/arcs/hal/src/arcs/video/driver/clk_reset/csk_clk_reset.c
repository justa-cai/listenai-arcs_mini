#include "csk_clk_reset.h"

/************** AP_CFG RESET ************************************************/
void ap_cfg_gpdma_reset(void)
{
    //IP_AP_CFG->REG_SW_RESET.bit.DMAC_GP_RESET = 0x1;    // bit1
    IP_AP_CFG->REG_SW_RESET.all |= (1<<1);
}

void ap_cfg_video_reset(void)
{
    //IP_AP_CFG->REG_SW_RESET.bit.VIDEO_RESET = 0x1;      // bit3
    IP_AP_CFG->REG_SW_RESET.all |= (1<<3);
}

void ap_cfg_vic_reset(void)
{
    //IP_AP_CFG->REG_SW_RESET.bit.VIC_RESET = 0x1;        // bit9
    IP_AP_CFG->REG_SW_RESET.all |= (1<<9);
}

void ap_cfg_qspi0_reset(void)
{
    //IP_AP_CFG->REG_SW_RESET.bit.QSPI0_RESET = 0x1;      // bit10
    IP_AP_CFG->REG_SW_RESET.all |= (1<<10);
}

void ap_cfg_qspi1_reset(void)
{
    //IP_AP_CFG->REG_SW_RESET.bit.QSPI1_RESET = 0x1;      // bit11
    IP_AP_CFG->REG_SW_RESET.all |= (1<<11);
}

void ap_cfg_rgb_reset(void)
{
    //IP_AP_CFG->REG_SW_RESET.bit.RGB_RESET = 0x1;        // bit12
    IP_AP_CFG->REG_SW_RESET.all |= (1<<12);
}

void ap_cfg_blender_reset(void)
{
    //IP_AP_CFG->REG_SW_RESET.bit.BLENDER_RESET = 0x1;    // bit13
    IP_AP_CFG->REG_SW_RESET.all |= (1<<13);
}

void ap_cfg_jpeg_reset(void)
{
    //IP_AP_CFG->REG_SW_RESET.bit.JPEG_RESET = 0x1;       // bit14
    IP_AP_CFG->REG_SW_RESET.all |= (1<<14);
}


/************** AP_CFG CLK ENABLE ************************************************/
void ap_cfg_gpdma_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 0x1;  // bit14
    IP_AP_CFG->REG_CLK_CFG0.all |= (1<<14);
}

void ap_cfg_video_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x1;  // bit15
    IP_AP_CFG->REG_CLK_CFG0.all |= (1<<15);
}

void ap_cfg_vic_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIC_CLK = 0x1;  // bit21
    IP_AP_CFG->REG_CLK_CFG0.all |= (1<<21);
}

void ap_cfg_rgb_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.ENA_RGB_CLK = 0x1;  // bit23
    IP_AP_CFG->REG_CLK_CFG0.all |= (1<<23);
}

void ap_cfg_qspi0_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.ENA_QSPI0_CLK = 0x1;  // bit11
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<11);
}

void ap_cfg_qspi1_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.ENA_QSPI1_CLK = 0x1;  // bit21
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<21);
}

void ap_cfg_blender_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.ENA_BLENDER_CLK = 0x1;  // bit30
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<30);
}

void ap_cfg_jpeg_clk_enable(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.ENA_JPEG_CLK = 0x1;  // bit31
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<31);
}


/************** AP_CFG CLK DISABLE ************************************************/
void ap_cfg_gpdma_clk_disable(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 0x1;  // bit14
    IP_AP_CFG->REG_CLK_CFG0.all &= ~(1<<14);
}

void ap_cfg_video_clk_disable(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x1;  // bit15
    IP_AP_CFG->REG_CLK_CFG0.all &= ~(1<<15);
}

void ap_cfg_vic_clk_disable(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIC_CLK = 0x1;  // bit21
    IP_AP_CFG->REG_CLK_CFG0.all &= ~(1<<21);
}

void ap_cfg_rgb_clk_disable(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.ENA_RGB_CLK = 0x1;  // bit23
    IP_AP_CFG->REG_CLK_CFG0.all &= ~(1<<23);
}

void ap_cfg_qspi0_clk_disable(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.ENA_QSPI0_CLK = 0x1;  // bit11
    IP_AP_CFG->REG_CLK_CFG1.all &= ~(1<<11);
}

void ap_cfg_qspi1_clk_disable(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.ENA_QSPI1_CLK = 0x1;  // bit21
    IP_AP_CFG->REG_CLK_CFG1.all &= ~(1<<21);
}

void ap_cfg_blender_clk_disable(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.ENA_BLENDER_CLK = 0x1;  // bit30
    IP_AP_CFG->REG_CLK_CFG1.all &= ~(1<<30);
}

void ap_cfg_jpeg_clk_disable(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.ENA_JPEG_CLK = 0x1;  // bit31
    IP_AP_CFG->REG_CLK_CFG1.all &= ~(1<<31);
}


/************** AP_CFG CLK SEL ************************************************/
void ap_cfg_vic_clk_inv(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.EDGE_SEL_VIC_CLK = 0x1;  // bit22  0:normal  1:inv
    IP_AP_CFG->REG_CLK_CFG0.all |= (1<<22);
}

void ap_cfg_rgb_clk_sel_24(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.SEL_RGB_CLK = 0x1;  // bit24  0:24MHz  1:syspll_peri_clk
    IP_AP_CFG->REG_CLK_CFG0.all &= ~(1<<24);
}

void ap_cfg_rgb_clk_sel_sys(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.SEL_RGB_CLK = 0x1;  // bit24  0:24MHz  1:syspll_peri_clk
    IP_AP_CFG->REG_CLK_CFG0.all |= (1<<24);
}

void ap_cfg_rgb_clk_div_m(uint8_t div)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.DIV_RGB_CLK_M = 0x1;  // bit25~28
    IP_AP_CFG->REG_CLK_CFG0.all &= ~(0xF << 25);
    IP_AP_CFG->REG_CLK_CFG0.all |= ((div & 0xF) << 25);
}

void ap_cfg_rgb_clk_inv(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.DIV_RGB_CLK_LD = 0x1;  // bit30  0:normal  1:inv
    IP_AP_CFG->REG_CLK_CFG0.all |= (1<<30);
}

void ap_cfg_rgb_clk_normal(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.DIV_RGB_CLK_LD = 0x1;  // bit30  0:normal  1:inv
    IP_AP_CFG->REG_CLK_CFG0.all &= ~(1<<30);
}

void ap_cfg_rgb_clk_ld(void)
{
    //IP_AP_CFG->REG_CLK_CFG0.bit.DIV_RGB_CLK_LD = 0x1;  // bit29  0:normal  1:inv
    IP_AP_CFG->REG_CLK_CFG0.all |= (1<<29);
}


void ap_cfg_qspi0_clk_sel(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.SEL_QSPI0_CLK = 0x1;  // bit10  0:XTAL  1:syspll_peri_clk
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<10);
}

void ap_cfg_qspi0_clk_div_m(uint8_t div)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI0_CLK_M = 0x1;  // bit12~15
    IP_AP_CFG->REG_CLK_CFG1.all &= ~(0xF << 12);
    IP_AP_CFG->REG_CLK_CFG1.all |= ((div & 0xF) << 12);
}

void ap_cfg_qspi0_clk_div_n(uint8_t div)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI0_CLK_N = 0x1;  // bit16~18
    IP_AP_CFG->REG_CLK_CFG1.all &= ~(0x7 << 16);
    IP_AP_CFG->REG_CLK_CFG1.all |= ((div & 0x7) << 16);
}

void ap_cfg_qspi0_clk_inv(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI0_CLK_LD = 0x1;  // bit19  0:normal  1:inv
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<19);
}


void ap_cfg_qspi1_clk_sel(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.SEL_QSPI1_CLK = 0x1;  // bit20  0:XTAL  1:syspll_peri_clk
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<20);
}

void ap_cfg_qspi1_clk_div_m(uint8_t div)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI1_CLK_M = 0x1;  // bit22~25
    IP_AP_CFG->REG_CLK_CFG1.all &= ~(0xF << 22);
    IP_AP_CFG->REG_CLK_CFG1.all |= ((div & 0xF) << 22);
}

void ap_cfg_qspi1_clk_div_n(uint8_t div)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI1_CLK_N = 0x1;  // bit26~28
    IP_AP_CFG->REG_CLK_CFG1.all &= ~(0x7 << 26);
    IP_AP_CFG->REG_CLK_CFG1.all |= ((div & 0x7) << 26);
}

void ap_cfg_qspi1_clk_inv(void)
{
    //IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI1_CLK_LD = 0x1;  // bit29  0:normal  1:inv
    IP_AP_CFG->REG_CLK_CFG1.all |= (1<<29);
}


void ap_cfg_dma_sel_qspi_in(void)
{
    IP_AP_CFG->REG_DMA_SEL.all &= ~(1<<1);     // bit1  0:qspi_in  1:dvp
}

void ap_cfg_dma_sel_dvp(void)
{
    IP_AP_CFG->REG_DMA_SEL.all |= (1<<1);      // bit1  0:qspi_in  1:dvp
}

void ap_cfg_dma_sel_rgb(void)
{
    IP_AP_CFG->REG_DMA_SEL.all &= ~(1<<0);     // bit0  0:rgb  1:qspi_out
}

void ap_cfg_dma_sel_qspi_out(void)
{
    IP_AP_CFG->REG_DMA_SEL.all |= (1<<0);      // bit0  0:rgb  1:qspi_out
    //IP_AP_CFG->REG_DMA_SEL.all |= (1<<2);      // bit2  0:rgb  1:qspi_out
}


void ap_cfg_reg_dump(void)
{
    VIDEO_LOG("REG_SW_RESET          *0x%08x = 0x%08x", &IP_AP_CFG->REG_SW_RESET.all, IP_AP_CFG->REG_SW_RESET.all);
    VIDEO_LOG("REG_AP_RESET          *0x%08x = 0x%08x", &IP_AP_CFG->REG_AP_RESET.all, IP_AP_CFG->REG_AP_RESET.all);
    VIDEO_LOG("REG_CLK_CFG0          *0x%08x = 0x%08x", &IP_AP_CFG->REG_CLK_CFG0.all, IP_AP_CFG->REG_CLK_CFG0.all);
    VIDEO_LOG("REG_CLK_CFG1          *0x%08x = 0x%08x", &IP_AP_CFG->REG_CLK_CFG1.all, IP_AP_CFG->REG_CLK_CFG1.all);
    VIDEO_LOG("REG_ECC_CTRL          *0x%08x = 0x%08x", &IP_AP_CFG->REG_ECC_CTRL.all, IP_AP_CFG->REG_ECC_CTRL.all);
    VIDEO_LOG("REG_MCU_AP_RST_ADDR   *0x%08x = 0x%08x", &IP_AP_CFG->REG_MCU_AP_RST_ADDR.all, IP_AP_CFG->REG_MCU_AP_RST_ADDR.all);
    VIDEO_LOG("REG_MCU_AP_CFG        *0x%08x = 0x%08x", &IP_AP_CFG->REG_MCU_AP_CFG.all, IP_AP_CFG->REG_MCU_AP_CFG.all);
    VIDEO_LOG("REG_DMA_SEL           *0x%08x = 0x%08x", &IP_AP_CFG->REG_DMA_SEL.all, IP_AP_CFG->REG_DMA_SEL.all);
}


#if 0
/************** image proc ************************************************/
void image_proc_enable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x00, 1);   // 0:disable  1:enable
}

void image_proc_vic_enable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x04, 1);   // 0:disable  1:enable
}

void image_proc_jpeg_enable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x08, 1);   // 0:disable  1:enable
}

void image_proc_blender_enable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x0C, 1);   // 0:disable  1:enable
}

void image_proc_display_enable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x10, 1);   // 0:disable  1:enable
}

void image_proc_qspi_enable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x14, 1);   // 0:disable  1:enable
}

void image_proc_rgb_enable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x1C, 1);   // 0:disable  1:enable
}

void image_proc_disable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x00, 0);   // 0:disable  1:enable
}

void image_proc_vic_disable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x04, 0);   // 0:disable  1:enable
}

void image_proc_jpeg_disable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x08, 0);   // 0:disable  1:enable
}

void image_proc_blender_disable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x0C, 0);   // 0:disable  1:enable
}

void image_proc_display_disable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x10, 0);   // 0:disable  1:enable
}

void image_proc_qspi_disable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x14, 0);   // 0:disable  1:enable
}

void image_proc_rgb_disable(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x1C, 0);   // 0:disable  1:enable
}

void image_proc_sel_dvp(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x18, 1);   // 0:qspi  1:dvp
}

void image_proc_sel_qspi(void)
{
    mmio_write32(IMAGE_PROC_BASE + 0x18, 0);   // 0:qspi  1:dvp
}


void image_proc_reg_dump(void)
{
    VIDEO_LOG("IMAGE_PROC *0x%08x = 0x%08x", IMAGE_PROC_BASE + 0x00, mmio_read32(IMAGE_PROC_BASE + 0x00));
    VIDEO_LOG("IMAGE_PROC *0x%08x = 0x%08x", IMAGE_PROC_BASE + 0x04, mmio_read32(IMAGE_PROC_BASE + 0x04));
    VIDEO_LOG("IMAGE_PROC *0x%08x = 0x%08x", IMAGE_PROC_BASE + 0x08, mmio_read32(IMAGE_PROC_BASE + 0x08));
    VIDEO_LOG("IMAGE_PROC *0x%08x = 0x%08x", IMAGE_PROC_BASE + 0x0C, mmio_read32(IMAGE_PROC_BASE + 0x0C));
    VIDEO_LOG("IMAGE_PROC *0x%08x = 0x%08x", IMAGE_PROC_BASE + 0x10, mmio_read32(IMAGE_PROC_BASE + 0x10));
    VIDEO_LOG("IMAGE_PROC *0x%08x = 0x%08x", IMAGE_PROC_BASE + 0x14, mmio_read32(IMAGE_PROC_BASE + 0x14));
    VIDEO_LOG("IMAGE_PROC *0x%08x = 0x%08x", IMAGE_PROC_BASE + 0x18, mmio_read32(IMAGE_PROC_BASE + 0x18));
    VIDEO_LOG("IMAGE_PROC *0x%08x = 0x%08x", IMAGE_PROC_BASE + 0x1C, mmio_read32(IMAGE_PROC_BASE + 0x1C));
}
#endif


