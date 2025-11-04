#ifndef __SW_QSPI_H_
#define __SW_QSPI_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


typedef struct {
    void     (*gpio_init)       (void);
    void     (*cs_set)          (void);
    void     (*dc_set)          (void);
    void     (*clk_set)         (void);
    void     (*d0_set)          (void);
    void     (*d1_set)          (void);
    void     (*d2_set)          (void);
    void     (*d3_set)          (void);
    void     (*cs_clr)          (void);
    void     (*dc_clr)          (void);
    void     (*clk_clr)         (void);
    void     (*d0_clr)          (void);
    void     (*d1_clr)          (void);
    void     (*d2_clr)          (void);
    void     (*d3_clr)          (void);
    void     (*delay_ms)        (unsigned int nms);
    void     (*delay_us)        (unsigned int nus);
    int      (*log)             (const char* format, ...);
} sw_qspi_out_port_callback_t;

void sw_qspi_init(sw_qspi_out_port_callback_t *callback);
void sw_qspi_write8_1lane(unsigned char dat);
void sw_qspi_write8_2lane(unsigned char dat);
void sw_qspi_write8_4lane(unsigned char dat);
void sw_qspi_write_buf_1lane(void *pdata, unsigned int num);

void sw_qspi_cs_set(void);
void sw_qspi_cs_clr(void);


#ifdef __cplusplus
}
#endif

#endif
