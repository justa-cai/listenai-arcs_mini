#ifndef __DRIVER_DVP_INTERNAL_H
#define __DRIVER_DVP_INTERNAL_H

#include "arcs_ap.h"
#include "ClockManager.h"
#include "log_print.h"

#include "image_vic_reg.h"
#include "Driver_DVP.h"


#if  (IC_BOARD==0)  // FPGA
#define CSK_DVP_HCLK_HZ                     (75000000)                                 /*!< 75MHz */
#else
#define CSK_DVP_HCLK_HZ                     (300000000)                                /*!< 300MHz */
#endif

#define CSK_DVP_DIV_MAX                     (0x3F)                                     /*!< 63 */
#define CSK_DVP_OUTPUT_CLK_HZ_MAX           (CSK_DVP_HCLK_HZ / 2)
#define CSK_DVP_OUTPUT_CLK_HZ_MIN           ((CSK_DVP_HCLK_HZ / 2) / (CSK_DVP_DIV_MAX + 1))

#define CSK_DVP_DMA_BURST_THD               (8)                                       /*!< 0~63 0x3F */


/********************  Bits definition for DVP_INPUT_FORM register  *******************/
#define CSK_DVP_INPUT_FORM_YUV422_Y0CBY1CR        ((0 << 2) | 0x0U)
#define CSK_DVP_INPUT_FORM_YUV422_CBY0CRY1        ((0 << 2) | 0x1U)
#define CSK_DVP_INPUT_FORM_YUV422_Y0CRY1CB        ((0 << 2) | 0x2U)
#define CSK_DVP_INPUT_FORM_YUV422_CRY0CBY1        ((0 << 2) | 0x3U)
#define CSK_DVP_INPUT_FORM_YUV420_Y0CBY1Y2CRY3    ((1 << 2) | 0x0U)
#define CSK_DVP_INPUT_FORM_YUV420_CBY0Y1CRY2Y3    ((1 << 2) | 0x1U)
#define CSK_DVP_INPUT_FORM_YUV420_Y0Y1CBY2Y3CR    ((1 << 2) | 0x2U)
#define CSK_DVP_INPUT_FORM_YUV444_Y0CBCR          ((2 << 2) | 0x0U)
#define CSK_DVP_INPUT_FORM_LUMINA_12BIT           ((3 << 2) | 0x0U)
#define CSK_DVP_INPUT_FORM_LUMINA_10BIT           ((3 << 2) | 0x1U)
#define CSK_DVP_INPUT_FORM_LUMINA_8BIT            ((3 << 2) | 0x2U)

/********************  Bits definition for DVP_POL_CNTL register  *********************/
#define CSK_DVP_POL_CNTL_CLOCK_RISING       (0x0UL)
#define CSK_DVP_POL_CNTL_CLOCK_FALLING      (0x1UL)

#define CSK_DVP_POL_CNTL_HSYNC_RISING       (0x0UL)
#define CSK_DVP_POL_CNTL_HSYNC_FALLING      (0x1UL)

#define CSK_DVP_POL_CNTL_VSYNC_RISING       (0x0UL)
#define CSK_DVP_POL_CNTL_VSYNC_FALLING      (0x1UL)

#define CSK_DVP_DATA_BUS_ALIGN_LSB          (0x0UL)
#define CSK_DVP_DATA_BUS_ALIGN_MSB          (0x1UL)

/**
  * @brief  DVP handle Structure definition
  */
typedef struct __DVP_DEV
{
    IMAGE_VIC_RegDef              *Instance;           /*!< DVP Register base address  */

    DVP_InitTypeDef               Init;                /*!< DVP parameters             */

    CSK_DVP_SignalEvent_t         cb_event;            /*!< DVP callback               */

    volatile DVP_emState          State;               /*!< DVP state                  */

    volatile uint32_t             ErrorCode;           /*!< DVP Error code             */
}DVP_DEV;


#endif /* __DRIVER_DVP_INTERNAL_H */


