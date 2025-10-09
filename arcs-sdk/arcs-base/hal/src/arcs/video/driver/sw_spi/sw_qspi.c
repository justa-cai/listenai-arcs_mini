#include "sw_qspi.h"
#include "config.h"

sw_qspi_out_port_callback_t sw_qspi_out_port_callback = {0};

#define SW_QSPI_CS_Set()        sw_qspi_out_port_callback.cs_set()
#define SW_QSPI_CS_Clr()        sw_qspi_out_port_callback.cs_clr()

#define SW_QSPI_SCK_Set()       sw_qspi_out_port_callback.clk_set()
#define SW_QSPI_SCK_Clr()       sw_qspi_out_port_callback.clk_clr()

#define SW_QSPI_SDA_Set()       sw_qspi_out_port_callback.d0_set()
#define SW_QSPI_SDA_Clr()       sw_qspi_out_port_callback.d0_clr()

#define SW_QSPI_D1_Set()        sw_qspi_out_port_callback.d1_set()
#define SW_QSPI_D1_Clr()        sw_qspi_out_port_callback.d1_clr()

#define SW_QSPI_D2_Set()        sw_qspi_out_port_callback.d2_set()
#define SW_QSPI_D2_Clr()        sw_qspi_out_port_callback.d2_clr()

#define SW_QSPI_D3_Set()        sw_qspi_out_port_callback.d3_set()
#define SW_QSPI_D3_Clr()        sw_qspi_out_port_callback.d3_clr()

//#define SW_QSPI_DELAY_MS(_nms)  sw_qspi_out_port_callback.delay_ms(_nms)
//#define SW_QSPI_DELAY_US(_nus)  sw_qspi_out_port_callback.delay_us(_nus)
//#define SW_QSPI_LOG(fmt, ...)   sw_qspi_out_port_callback.log(fmt, ##__VA_ARGS__)
#define SW_QSPI_DELAY_MS(_nms)  DELAY_MS(_nms)
#define SW_QSPI_DELAY_US(_nus)  DELAY_US(_nus)
#define SW_QSPI_LOG(fmt, ...)   VIDEO_LOG(fmt, ##__VA_ARGS__)


void sw_qspi_init(sw_qspi_out_port_callback_t *callback)
{
    if(NULL == callback) {
        return;
    }

    memcpy(&sw_qspi_out_port_callback, callback, sizeof(sw_qspi_out_port_callback_t));
    SW_QSPI_LOG("[%s:%d]", __func__, __LINE__);
    sw_qspi_out_port_callback.gpio_init();
}


/*******************************************************************************************************/
void sw_qspi_cs_set(void)
{
    SW_QSPI_CS_Set();
}


void sw_qspi_cs_clr(void)
{
    SW_QSPI_CS_Clr();
}


void sw_qspi_write8_1lane(unsigned char dat)
{
    unsigned char i;

    for(i=0;i<8;i++)
    {
        SW_QSPI_SCK_Clr();
        if(dat&0x80)
        {
            SW_QSPI_SDA_Set();
        }
        else
        {
            SW_QSPI_SDA_Clr();
        }
        SW_QSPI_SCK_Set();
        dat<<=1;
    }
}


void sw_qspi_write8_4lane(unsigned char i)
{
    SW_QSPI_SCK_Clr();

    if(i&0x80)
        SW_QSPI_D3_Set();
    else
        SW_QSPI_D3_Clr();

    if(i&0x40)
        SW_QSPI_D2_Set();
    else
        SW_QSPI_D2_Clr();

    if(i&0x20)
        SW_QSPI_D1_Set();
    else
        SW_QSPI_D1_Clr();

    if(i&0x10)
        SW_QSPI_SDA_Set();
    else
        SW_QSPI_SDA_Clr();

    SW_QSPI_SCK_Set();

    SW_QSPI_SCK_Clr();

    if(i&0x08)
        SW_QSPI_D3_Set();
    else
        SW_QSPI_D3_Clr();

    if(i&0x04)
        SW_QSPI_D2_Set();
    else
        SW_QSPI_D2_Clr();

    if(i&0x02)
        SW_QSPI_D1_Set();
    else
        SW_QSPI_D1_Clr();

    if(i&0x01)
        SW_QSPI_SDA_Set();
    else
        SW_QSPI_SDA_Clr();

    SW_QSPI_SCK_Set();
}


void sw_qspi_write8_2lane(unsigned char i)
{
    SW_QSPI_SCK_Clr();
    if(i&0x80)
        SW_QSPI_D1_Set();
    else
        SW_QSPI_D1_Clr();
    if(i&0x40)
        SW_QSPI_SDA_Set();
    else
        SW_QSPI_SDA_Clr();
    SW_QSPI_SCK_Set();

    SW_QSPI_SCK_Clr();
    if(i&0x20)
        SW_QSPI_D1_Set();
    else
        SW_QSPI_D1_Clr();
    if(i&0x10)
        SW_QSPI_SDA_Set();
    else
        SW_QSPI_SDA_Clr();
    SW_QSPI_SCK_Set();

    SW_QSPI_SCK_Clr();
    if(i&0x08)
        SW_QSPI_D1_Set();
    else
        SW_QSPI_D1_Clr();
    if(i&0x04)
        SW_QSPI_SDA_Set();
    else
        SW_QSPI_SDA_Clr();
    SW_QSPI_SCK_Set();

    SW_QSPI_SCK_Clr();
    if(i&0x02)
        SW_QSPI_D1_Set();
    else
        SW_QSPI_D1_Clr();
    if(i&0x01)
        SW_QSPI_SDA_Set();
    else
        SW_QSPI_SDA_Clr();
    SW_QSPI_SCK_Set();
}


void sw_qspi_write_buf_1lane(void *pdata, unsigned int num)
{
    unsigned int i = 0;
    unsigned char *pbuf = (unsigned char *)pdata;

    for(i = 0; i < num; i++)
    {
        sw_qspi_write8_1lane(*pbuf++);
    }
}

