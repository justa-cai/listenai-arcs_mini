#include "dma.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define MASTER_SEL_CNT  4
#define SRC_MASTER_SEL  0 // only support Master 0 on AP (Options: 0 ~ 3)
#define DST_MASTER_SEL  0 // only support Master 0 on AP (Options: 0 ~ 3)

#define SMS_NO  SRC_MASTER_SEL
#define DMS_NO  DST_MASTER_SEL


// DMA 事件标志
static uint32_t volatile DMAEvent;

// 源/目的缓冲区的最大长度
#define MaxLen          1024 // 1024*5

// 源缓冲区大小
#define SourceLen       (MaxLen-1) //100 

// 目的缓冲区大小
#define ReceiveLen      (MaxLen-1) //100 

// 单位长度（字节）
#define UnitLen         4

// 定义源/目的缓冲区（使用32位整型数组）
static uint32_t SourceBuf[MaxLen] = {0};
static uint32_t DestinBuf[MaxLen] = {0};

//=============================================================================
// 生成源缓冲区内容（顺序填充0~255）
static void
DMA_Src_Buf_Gen(uint8_t *buffer, uint32_t size)
{
    int i;
    for (i = 0; i < size; i++)
    {
        buffer[i] = i % 256;
    }
}
// 生成目的缓冲区内容（填充平方值 % 256）
static void
DMA_Dst_Buf_Gen(uint8_t *buffer, uint32_t size)
{
    int i = 0;
    for (i = 0; i < size; i++)
    {
        buffer[i] = (i * i) % 256;
    }
}
// DMA 事件回调函数
static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    printf("[%s]: event = %d, channel = %d, xfer_bytes = %d\r\n", __func__,
           event_info & 0xFF, (event_info >> 8) & 0xFF, xfer_bytes);
    DMAEvent = event_info & 0xFF;
}
// 等待 DMA 完成传输
static void
DMA_Waiting(void)
{
    while (1)
    {
        if (DMAEvent & DMA_EVENT_TRANSFER_COMPLETE)
        {
            DMAEvent &= (~DMA_EVENT_TRANSFER_COMPLETE);
            break;
        }
    }
}
//=============================================================================
// 初始化源缓冲区与目的缓冲区的数据
void DMA_mem_int(uint32_t total_bytes)//初始化源缓冲区与目的缓冲区打大小
{
    DMA_Src_Buf_Gen((uint8_t *)SourceBuf, total_bytes);
    DMA_Dst_Buf_Gen((uint8_t *)DestinBuf, total_bytes);
}

// DMA 配置检查：包括缓存一致性、源/目的地址宽度、地址增减设置等
// 返回内存比较结果：true 表示传输正确，false 表示错误或未成功启动 DMA
bool DMA_configure_check(uint8_t *pch, uint32_t src_width, uint32_t src_bsize,
                         uint32_t dst_width, uint32_t dst_bsize,
                         bool addr_dec, DMA_CACHE_SYNC cache_sync)
{
    int stat = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr, dst_addr, total_bytes = SourceLen * UnitLen;

    uint32_t print_size = 1 * UnitLen;//只打印1个单位数据
    DMA_mem_int(total_bytes);

    // 在 DMA 传输前访问源/目的缓冲区部分内容
    SourceBuf[0] = 0xAABBCCDD;
    DestinBuf[0] = 0x77889900;

    // 打印DMA传输前的源和目的缓冲区数据

    uint8_t *src_bytes = (uint8_t *)SourceBuf;
    uint8_t *dst_bytes = (uint8_t *)DestinBuf;
    printf("SourceBuf: ");
    for (size_t i = 0; i < print_size; i++)
    {
        printf("%02X ", src_bytes[i]);
    }
    printf("\n");
    // Before transfer
    printf("DestinBuf (Before): ");
    for (size_t i = 0; i < print_size; i++)
    {
        printf("%02X ", dst_bytes[i]);
    }
    printf("\n");
    src_addr = (uint32_t)SourceBuf;
    dst_addr = (uint32_t)DestinBuf;
    // 设置 DMA 控制参数
    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(dst_width) | DMA_CH_CTLL_SRC_WIDTH(src_width) |
              DMA_CH_CTLL_DST_BSIZE(dst_bsize) | DMA_CH_CTLL_SRC_BSIZE(src_bsize) |
              DMA_CH_CTLL_TTFC_M2M | DMA_CH_CTLL_DMS(DMS_NO) | DMA_CH_CTLL_SMS(SMS_NO);

    if (addr_dec)
    {
        src_addr += total_bytes - (0x1 << src_width);
        dst_addr += total_bytes - (0x1 << dst_width);
        control |= DMA_CH_CTLL_DST_DEC | DMA_CH_CTLL_SRC_DEC;
    }
    else
    {
        control |= DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_INC;
    }

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE;
    // 申请 DMA 通道
    uint8_t ch = dma_channel_select(pch, DMA_DrvEvent, 0, cache_sync);
    if (ch == DMA_CHANNEL_ANY)
    {
        printf("[FAILED] NO free DMA channel!!\r\n");
        return false;
    }
    // 配置 DMA 通道
    stat = dma_channel_configure(ch, src_addr, dst_addr, total_bytes / WIDTH_BYTES(src_width),
                                 control, config_low, config_high, 0, 0);

    if (stat == -1)
    {
        printf("[FAILED] dma_channel_configure error.\r\n");
        dma_channel_disable(ch, true); // release the selected DMA channel
        return false;
    }

    DMA_Waiting();
    // 打印 DMA 传输后的缓冲区数据
    printf("After DMA transfer:\n");
    src_bytes = (uint8_t *)SourceBuf;
    dst_bytes = (uint8_t *)DestinBuf;

    printf("DestinBuf (After ): ");
    for (size_t i = 0; i < print_size; i++)
    {
        printf("%02X ", dst_bytes[i]);
    }
    printf("\n");
    // 比较内存，判断是否传输一致
    if (memcmp(SourceBuf, DestinBuf, total_bytes) == 0)
    {
        printf("Memory compare success\n");
        return true;
    }
    else
    {
        printf("[%s FAILED] channel %d, Memory compare error !!!!!\r\n", __func__, ch);
        return false;
    }
}


int main(int argc, char **argv)
{
    // 初始化 DMA 控制器
    dma_initialize();
    printf("Hello, world! dma\n");
    // 配置 DMA：目标地址按字节对齐，突发大小为 64
    uint8_t ch = 0;// ch可取0-DMA_NUMBER_OF_CHANNELS
    DMA_configure_check(&ch, DMA_WIDTH_WORD, DMA_BSIZE_16, DMA_WIDTH_BYTE, DMA_BSIZE_64, false, DMA_CACHE_SYNC_AUTO);
    return 0;
}
