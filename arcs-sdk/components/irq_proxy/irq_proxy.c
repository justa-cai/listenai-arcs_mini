#include "irq_proxy.h"
#include "Driver_MBX.h"
#include <string.h>
#include "log_print.h"

#define IRQ_PROXY_MBX_DEV     NULL
#define IRQ_PROXY_MAX_CHANNELS 8
#define IRQ_PROXY_OFFSET 24

#define MEM_SHARED_BASE 0x20040000
#define MEM_SHARED_SIZE 0x1000

// 共享内存结构体定义
struct ap_cp_shared_struct {
    uint32_t irq_status;
    uint8_t shared_data[MEM_SHARED_SIZE];
    uint32_t ack_flag;
};

#define ap_cp_shared ((volatile struct ap_cp_shared_struct *)MEM_SHARED_BASE)

static irq_proxy_device_t g_irq_proxy_devices[IRQ_PROXY_MAX_CHANNELS];

static inline uint32_t co_clz(uint32_t val)
{
    uint32_t tmp;
    uint32_t shift = 0;

    if (val == 0)
    {
        return 32;
    }

    tmp = val >> 16;
    if (tmp)
    {
        shift = 16;
        val = tmp;
    }

    tmp = val >> 8;
    if (tmp)
    {
        shift += 8;
        val = tmp;
    }

    tmp = val >> 4;
    if (tmp)
    {
        shift += 4;
        val = tmp;
    }

    tmp = val >> 2;
    if (tmp)
    {
        shift += 2;
        val = tmp;
    }

    tmp = val >> 1;
    if (tmp)
    {
        shift += 1;
    }

    return (31 - shift);
}

/**
 * @brief IRQ代理中断处理函数
 * @param event 事件类型
 * @param param 通道号
 * @return 成功返回0,失败返回负值
 */
static int8_t irq_proxy_handler(uint32_t event, uint32_t param)
{
    while (param) {
        uint32_t highest_bit = 31 - co_clz(param);
        irq_proxy_channel_t channel = highest_bit - IRQ_PROXY_OFFSET;
        
        // 处理当前通道
        if (channel < IRQ_PROXY_MAX_CHANNELS) {
            irq_proxy_device_t *dev = &g_irq_proxy_devices[channel];
            if (dev->callback) {
                dev->callback();
            }
        }
        
        // 清除已处理的bit
        param &= ~(1U << highest_bit);
    }
    return 0;
}

/**
 * @brief 初始化IRQ代理
 */
void irq_proxy_init(void)
{
    memset(g_irq_proxy_devices, 0, sizeof(g_irq_proxy_devices));
    MBX3_Initialize(IRQ_PROXY_MBX_DEV, irq_proxy_handler);
}

/**
 * @brief 注册回调函数
 * @param channel 通道号
 * @param callback 回调函数
 * @return 成功返回0,失败返回负值
 */
int32_t irq_proxy_register_callback(irq_proxy_channel_t channel, irq_proxy_callback_fn callback)
{
    if (channel >= IRQ_PROXY_MAX_CHANNELS || !callback) {
        return -1;
    }

    g_irq_proxy_devices[channel].callback = callback;

    return 0;
}

/**
 * @brief 触发指定通道
 * @param channel 通道号
 * @return 成功返回0,失败返回负值
 */
int32_t irq_proxy_trigger(irq_proxy_channel_t channel)
{
    // 参数检查
    if (channel >= IRQ_PROXY_MAX_CHANNELS) {
        return -1;
    }

    return MBX_Trigger(IRQ_PROXY_MBX_DEV, channel + IRQ_PROXY_OFFSET);

}
