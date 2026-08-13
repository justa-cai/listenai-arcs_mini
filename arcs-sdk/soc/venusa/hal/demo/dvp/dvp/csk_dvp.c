#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "queue.h"
#include "list.h"

#include "venusa_ap.h"
#include "log_print.h"
#include "systick.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "Driver_DMA2D.h"
#include "Driver_I2C.h"
#include "Driver_DVP.h"
#include "csk_dvp.h"
#include "camera.h"
#include "check.h"

#define VIDEO_LOG  CLOG
#define DELAY_MS(x) SysTick_Delay_Ms(x)
#define DELAY_US(x) SysTick_Delay_Us(x)

#define DVP_GPDMA_CH            dma_2d_ch5
#define DVP_MCLK_OUT_HZ         24000000
#define DVP_IMAGE_WIDTH         1280
#define DVP_IMAGE_HEIGHT        720
#define DVP_IMAGE_SIZE_BYTE     (DVP_IMAGE_WIDTH * DVP_IMAGE_HEIGHT * 2)
#define DVP_MAX_BUF_NODES       5

typedef enum {
    DVP_BUF_UNUSED = 0,
    DVP_BUF_USED,
    DVP_BUF_DONE,
} BufState;

typedef struct BufNode {
    void *buf_addr;
    BufState state;
    ListItem_t list_item;
} BufNode_t;

static List_t buf_list;                                 // 缓冲区链表
static BufNode_t node_pool[DVP_MAX_BUF_NODES] = {0};    // 静态节点池
static uint8_t node_pool_used[DVP_MAX_BUF_NODES] = {0}; // 节点使用标记

static SemaphoreHandle_t dvpDoneSemaphore = NULL;       // DVP DMA done
static SemaphoreHandle_t dvpErrSemaphore = NULL;        // DVP DMA error
static SemaphoreHandle_t buf_list_mutex;                // 链表操作互斥量

static inline int32_t dvp_start(void);
static inline int32_t dvp_stop(void);
static inline int32_t dvp_gpdma_start(void *buf_addr);
static inline int32_t dvp_gpdma_stop(void);
static void dvp_callback(DVP_emIrqEvent event, uint32_t param);
static void dvp_gpdma_callback(uint32_t event, void *workspace);
static inline BufNode_t *get_free_node(void);
static inline void release_node(BufNode_t *node);
static inline void add_buf_node(BufNode_t *node);
static inline void remove_buf_node(BufNode_t *node);
static BufNode_t *find_first_done_node(void);
static BufNode_t *find_first_used_node(void);
static BufNode_t *find_first_unused_node(void);


static void dvp_callback(DVP_emIrqEvent event, uint32_t param)
{
    uint8_t error_flag = 0;

    switch(event)
    {
        case DVP_IRQ_EVENT_SOF:
            //VIDEO_LOG("[%s:%d] DVP SOF event: %d", __func__, __LINE__, event);
            break;

        case DVP_IRQ_EVENT_EOF:
//            if (*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_REMAIND_CNT_00.all + DVP_GPDMA_CH) != DVP_IMAGE_SIZE_BYTE) {
//                error_flag = 1;
//            }
            //VIDEO_LOG("[%s:%d] DVP EOF event: %d", __func__, __LINE__, event);
            break;

        case DVP_IRQ_EVENT_FRAME_FINISH:
            //VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FRAME_FINISH", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_FIFO_OVFLOW:
            //error_flag = 1;
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FIFO_OVFLOW", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_PIXEL_ABNOR:
            //error_flag = 1;
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_PIXEL_ABNOR", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_H_SYNC_ABNOR:
            //error_flag = 1;
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_H_SYNC_ABNOR", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_EOF_CNT_ABNOR:
            error_flag = 1;
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_EOF_CNT_ABNOR", __func__, __LINE__);
//            VIDEO_LOG("VIC_DEBUG    *0x%08x = 0x%08x", &IP_DVP_IN->REG_IMAGE_VIC_DEBUG.all, IP_DVP_IN->REG_IMAGE_VIC_DEBUG.all);
//            VIDEO_LOG("ST_DEBUG     *0x%08x = 0x%08x", &IP_DVP_IN->REG_ST_DEBUG.all, IP_DVP_IN->REG_ST_DEBUG.all);
//            VIDEO_LOG("SERSOR_DEBUG *0x%08x = 0x%08x", &IP_DVP_IN->REG_SERSOR_DEBUG.all, IP_DVP_IN->REG_SERSOR_DEBUG.all);
            break;

        case DVP_IRQ_EVENT_FIFO_WR_FULL:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FIFO_WR_FULL", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_DMA_VIC_SINGLE:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_DMA_VIC_SINGLE", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_DMA_VIC_REQ:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_DMA_VIC_REQ", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_FIFO_UNFLOW:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FIFO_UNFLOW", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_FIFO_RD_EMPTY:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FIFO_RD_EMPTY", __func__, __LINE__);
            break;

        default:
            VIDEO_LOG("[%s:%d] DVP error event: %d", __func__, __LINE__, event);
            break;
    }

    if (error_flag && (dvpErrSemaphore != NULL)) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(dvpErrSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

    return;
}

static void dvp_gpdma_callback(uint32_t event, void *workspace)
{
    //VIDEO_LOG("[%s:%d] event=0x%x", __func__, __LINE__, event);

    if(event & CSK_DMA2D_EVENT_BLOCK_DONE)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        // 获取互斥量（中断中使用FromISR版本）
        if (xSemaphoreTakeFromISR(buf_list_mutex, &xHigherPriorityTaskWoken) == pdTRUE)
        {
            // 查找第一个已使用的缓冲区
            BufNode_t *used_node = find_first_used_node();
            if (used_node != NULL) {
                used_node->state = DVP_BUF_DONE;

                // 通知应用层
                if (dvpDoneSemaphore != NULL) {
                    xSemaphoreGiveFromISR(dvpDoneSemaphore, &xHigherPriorityTaskWoken);
                    //xQueueSendFromISR(dvpQueue, &used_node->buf_addr, &xHigherPriorityTaskWoken);
                }
            }

            // 查找第一个未使用的缓冲区
            BufNode_t *unused_node = find_first_unused_node();
            if (unused_node != NULL) {
                // 设置为已使用并启动DMA2D
                unused_node->state = DVP_BUF_USED;
                dvp_gpdma_start(unused_node->buf_addr);
            }

            // 无可用缓冲区，停止DVP
            if (find_first_used_node() == NULL) {
                dvp_stop();
                dvp_gpdma_stop();
            }

            // 释放互斥量
            xSemaphoreGiveFromISR(buf_list_mutex, &xHigherPriorityTaskWoken);

            // 触发任务调度（如果需要）
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }

    if(event & CSK_DMA2D_EVENT_TRANSFER_DONE)
    {
//        dvp_stop();
//        dvp_gpdma_stop();
    }
}


int32_t dvp_init(SemaphoreHandle_t DoneSemaphore, SemaphoreHandle_t ErrSemaphore)
{
    int32_t ret = FAILURE;
    camera_config_t camera_cfg = {
        .sccb_i2c_port = 0,
        .xclk_freq_hz = DVP_MCLK_OUT_HZ,
        .pixel_format = PIXFORMAT_YUV422,
        .frame_size = FRAMESIZE_FHD,
        .colorbar = 0,
    };
    DVP_InitTypeDef dvp_cfg = {
        .FrameWidth = DVP_IMAGE_WIDTH,
        .FrameHeight = DVP_IMAGE_HEIGHT,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
        .BurstThreshold = 16,
    };
    csk_dma2d_init_t dma2d_para = {
        .dma_ch = DVP_GPDMA_CH,
        .tfr_mode = tfr_mode_p2m,
        .src_basic_unit = dma2d_sample_unit_word,
        .dst_basic_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_fix,
        .dst_inc_mode = inc_mode_increase,
        .src_burst_len = dma2d_burst_len_16spl,
        .dst_burst_len = dma2d_burst_len_16spl,
        .flow_ctrl = dma2d_flow_ctrl_dma,
        .prio_lvl = prio_mode_vhigh,
        .handshake = dvp_hs_num3,
        .src_gather.enable = csk_func_disable,
        .dst_scatter.enable = csk_func_disable,
        .trigger.mode = csk_trigger_null,
        .trigger.triggered_en = csk_func_disable,
        .trigger.triggered_src_chn = dma_2d_ch0
    };

    VIDEO_LOG("[%s:%d] imgae_w=%d imgae_h=%d", __func__, __LINE__, DVP_IMAGE_WIDTH, DVP_IMAGE_HEIGHT);

    // 初始化消息队列
    dvpDoneSemaphore = DoneSemaphore;
    dvpErrSemaphore = ErrSemaphore;

    // 初始化缓冲区链表
    vListInitialise(&buf_list);

    // 创建链表操作互斥量
    buf_list_mutex = xSemaphoreCreateMutex();

    // 链表参数初始化
    for (int i = 0; i < DVP_MAX_BUF_NODES; i++) {
        node_pool[i].list_item.pvOwner = &node_pool[i];
        node_pool[i].list_item.xItemValue = 0;
        node_pool_used[i] = 0;
    }

    /* pinmux */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_MCLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_PCLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_VS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_HS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 24, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 27, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 28, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D4
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 29, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D5
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 30, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D6
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 31, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D7
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  3, CSK_IOMUX_FUNC_DEFAULT);  // PIN_DVP_RST
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  2, CSK_IOMUX_FUNC_DEFAULT);  // PIN_DVP_PWDN
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  1, CSK_IOMUX_FUNC_ALTER7);   // PIN_DVP_SCL I2C0_SCL
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  0, CSK_IOMUX_FUNC_ALTER7);   // PIN_DVP_SDA I2C0_SDA

    /* PIN_DVP_RST */
    GPIO_SetDir(GPIOB(), (1UL << 3), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOB(), (1UL << 3), 0);
    DELAY_MS(100);
    GPIO_PinWrite(GPIOB(), (1UL << 3), 1);
    DELAY_MS(100);

    /* PIN_DVP_PWDN */
    GPIO_SetDir(GPIOB(), (1UL << 2), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOB(), (1UL << 2), 0);

    /* dvp init */
    ret = DVP_Initialize(DVP0(), dvp_callback, &dvp_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

    ret = DVP_EnableClockout(DVP_MCLK_OUT_HZ);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);
    DELAY_MS(100);

    /* camera init */
    camera_init(&camera_cfg);
    DELAY_MS(100);

    /* GPDMA init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

    ret = DMA2D_Config(&dma2d_para, dvp_gpdma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

    /* DMA handshake */
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_03 = 0;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_15 = 1;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_19 = 0;

	VIDEO_LOG("[%s:%d] SUCCESS", __func__, __LINE__);

    return ret;

error0:
    DVP_Stop(DVP0());
    DVP_Uninitialize(DVP0());
    DVP_DisableClockout();
    DMA2D_Stop(DVP_GPDMA_CH);

    VIDEO_LOG("[%s:%d] FAILED", __func__, __LINE__);

    return ret;
}


void dvp_buf_add(void *buf_addr)
{
    if (buf_addr == NULL) {
        VIDEO_LOG("[%s:%d] error: buf_addr is NULL", __func__, __LINE__);
        return;
    }

    // 创建新的缓冲区节点
    BufNode_t *new_node = get_free_node();
    if (new_node == NULL) {
        VIDEO_LOG("[%s:%d] FAILED: new node is NULL", __func__, __LINE__);
        return;
    }

    new_node->buf_addr = buf_addr;
    new_node->state = DVP_BUF_UNUSED;

    // 获取互斥量
    xSemaphoreTake(buf_list_mutex, portMAX_DELAY);

    // 添加节点到链表
    add_buf_node(new_node);

    // 获取已使用和未使用缓冲区数量
    uint8_t done_num, used_num, unused_num;
    dvp_buf_num_get(&done_num, &used_num, &unused_num);

    // 如果只有一个已使用缓冲区或无已使用缓冲区
    if ((used_num == 1) || (used_num == 0)) {
        new_node->state = DVP_BUF_USED;
        dvp_gpdma_start(new_node->buf_addr);

        if (used_num == 0) {
            dvp_start(); // 无已使用缓冲区时启动DVP
        }
    }

    // 释放互斥量
    xSemaphoreGive(buf_list_mutex);
}


void dvp_buf_get(void **buf_addr)
{
    if (buf_addr == NULL) {
        VIDEO_LOG("[%s:%d] error: buf_addr is NULL", __func__, __LINE__);
        return;
    }

    // 获取互斥量
    xSemaphoreTake(buf_list_mutex, portMAX_DELAY);

    BufNode_t *done_node = find_first_done_node();
    if (done_node != NULL) {
        *buf_addr = done_node->buf_addr;

        // 从链表移除节点
        done_node->buf_addr = NULL;
        remove_buf_node(done_node);
    } else {
        *buf_addr = NULL;
    }

    // 释放互斥量
    xSemaphoreGive(buf_list_mutex);
}


void dvp_buf_num_get(uint8_t *done_num, uint8_t *used_num, uint8_t *unused_num)
{
    *done_num = 0;
    *used_num = 0;
    *unused_num = 0;

    ListItem_t *pxIterator = listGET_HEAD_ENTRY(&buf_list);
    while (pxIterator != listGET_END_MARKER(&buf_list))
    {
        BufNode_t *node = (BufNode_t *)listGET_LIST_ITEM_OWNER(pxIterator);
        if (node->state == DVP_BUF_DONE) {
            (*done_num)++;
        } else if (node->state == DVP_BUF_USED) {
            (*used_num)++;
        } else {
            (*unused_num)++;
        }
        pxIterator = listGET_NEXT(pxIterator);
    }
}


static BufNode_t *find_first_done_node(void)
{
    ListItem_t *pxIterator = listGET_HEAD_ENTRY(&buf_list);
    while (pxIterator != listGET_END_MARKER(&buf_list))
    {
        BufNode_t *node = (BufNode_t *)listGET_LIST_ITEM_OWNER(pxIterator);
        if (node->state == DVP_BUF_DONE) {
            return node;
        }
        pxIterator = listGET_NEXT(pxIterator);
    }
    return NULL;
}


static BufNode_t *find_first_used_node(void)
{
    ListItem_t *pxIterator = listGET_HEAD_ENTRY(&buf_list);
    while (pxIterator != listGET_END_MARKER(&buf_list))
    {
        BufNode_t *node = (BufNode_t *)listGET_LIST_ITEM_OWNER(pxIterator);
        if (node->state == DVP_BUF_USED) {
            return node;
        }
        pxIterator = listGET_NEXT(pxIterator);
    }
    return NULL;
}


static BufNode_t *find_first_unused_node(void)
{
    ListItem_t *pxIterator = listGET_HEAD_ENTRY(&buf_list);
    while (pxIterator != listGET_END_MARKER(&buf_list))
    {
        BufNode_t *node = (BufNode_t *)listGET_LIST_ITEM_OWNER(pxIterator);
        if (node->state == DVP_BUF_UNUSED) {
            return node;
        }
        pxIterator = listGET_NEXT(pxIterator);
    }
    return NULL;
}


static inline void add_buf_node(BufNode_t *node)
{
    vListInsertEnd(&buf_list, &node->list_item);
}

static inline void remove_buf_node(BufNode_t *node)
{
    uxListRemove(&node->list_item);
    release_node(node);
}

static inline BufNode_t *get_free_node(void)
{
    for (int i = 0; i < DVP_MAX_BUF_NODES; i++)
    {
        if (node_pool_used[i] == 0) {
            node_pool_used[i] = 1;
            return &node_pool[i];
        }
    }
    return NULL;
}

static inline void release_node(BufNode_t *node)
{
    for (int i = 0; i < DVP_MAX_BUF_NODES; i++)
    {
        if (&node_pool[i] == node) {
            node_pool_used[i] = 0;
            break;
        }
    }
}


static inline int32_t dvp_start(void)
{
    //VIDEO_LOG("[%s:%d] ", __func__, __LINE__);
    int ret = DVP_Start(DVP0());
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    return ret;
}

static inline int32_t dvp_stop(void)
{
    //VIDEO_LOG("[%s:%d] ", __func__, __LINE__);
    int ret = DVP_Stop(DVP0());
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    return ret;
}

static inline int32_t dvp_gpdma_start(void *buf_addr)
{
    //VIDEO_LOG("[%s:%d] buf_addr=0x%x", __func__, __LINE__, buf_addr);
    int ret = DMA2D_Start_Normal(DVP_GPDMA_CH, (void *)DVP0_Buf(), buf_addr, DVP_IMAGE_SIZE_BYTE);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    return ret;
}

static inline int32_t dvp_gpdma_stop(void)
{
    //VIDEO_LOG("[%s:%d] ", __func__, __LINE__);
    int ret = DMA2D_Stop(DVP_GPDMA_CH);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);
    return ret;
}


