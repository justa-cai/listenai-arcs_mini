#ifndef __GPADC_H
#define __GPADC_H

#include "venusa_ap.h"
#include "Driver_GPADC.h"

#include "dma.h"

// GPADC flags
#define GPADC_FLAG_INITIALIZED          (1U << 0)
#define GPADC_FLAG_POWERED              (1U << 1)
#define GPADC_FLAG_CONFIGURED           (1U << 2)

typedef struct _GPADC_INFO
{
	uint32_t flags;         		   	// GPADC driver flags
	CSK_GPADC_SignalEvent_t cb_event[CSK_GPADC_CHANNEL_NUM]; 	// Event callback
	CSK_GPADC_SignalEvent_t cmp_event;
	uint32_t clk_source[2];
	void (*origin_IRQ_handler)(void);
} GPADC_INFO;

// GPADC Resource Configuration
typedef struct
{
    GPADC_RegDef* reg;                	  // GPADC register interface
    uint32_t irq_num;
    void (*irq_handler)(void);
    GPADC_INFO* info;               	  // Run-Time control information
    uint32_t user_param;
}GPADC_RESOURCES;


#define CSK_ADC_ISR_COMPLETE_Pos		3
#define CSK_ADC_ISR_FIFO_EMPTY_Pos		16
#define CSK_ADC_ISR_FIFO_FULL_Pos		0
#define CSK_ADC_ISR_FIFO_THD_Pos		16


#endif /* __GPADC_H */
