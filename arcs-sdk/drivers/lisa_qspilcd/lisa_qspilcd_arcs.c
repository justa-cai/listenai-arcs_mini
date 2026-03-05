#include <string.h>
#include "lisa_gpio.h"
#include "lisa_device.h"
#include "lisa_qspilcd.h"

#include "Driver_GPDMA.h"
#include "Driver_QSPI_LCD.h"
#include "lisa_semaphore.h"
#include "lisa_mutex.h"
#include "board.h"

#define LOG_TAG "lisa_qspilcd"
#include "lisa_log.h"

typedef struct {
    void *hal_handler;
    lisa_qspilcd_config_t config;
    lisa_device_t *cs_gpio;
    uint32_t cs_pin;
    lisa_mutex_t *lock;
    lisa_semaphore_t *dma_sem;
} lisa_qspilcd_priv_t;

#define QSPILCD_LOCK(priv)                                                                                             \
    do {                                                                                                               \
        if ((priv)->lock) {                                                                                            \
            lisa_mutex_lock((priv)->lock, LISA_OS_WAIT_FOREVER);                                                       \
        }                                                                                                              \
    } while (0)

#define QSPILCD_UNLOCK(priv)                                                                                           \
    do {                                                                                                               \
        if ((priv)->lock) {                                                                                            \
            lisa_mutex_unlock((priv)->lock);                                                                           \
        }                                                                                                              \
    } while (0)

static em_CSK_QSPI_LCD_LANE_NUM to_hal_lane(lisa_qspilcd_lane_num_t lane)
{
    switch (lane) {
    case LISA_QSPILCD_LANE_DUAL:
        return CSK_QSPI_LCD_LANE_NUM_DUAL;
    case LISA_QSPILCD_LANE_QUAD:
        return CSK_QSPI_LCD_LANE_NUM_QUAD;
    case LISA_QSPILCD_LANE_SINGLE:
    default:
        return CSK_QSPI_LCD_LANE_NUM_SINGLE;
    }
}

static inline int hal_status_to_err(int status)
{
    return (status == CSK_DRIVER_OK) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_IO;
}

static void qspilcd_dma_callback(uint32_t event, void *workspace)
{
    lisa_qspilcd_priv_t *priv = (lisa_qspilcd_priv_t *)workspace;
    if (!priv || !priv->dma_sem) {
        return;
    }

    if (event & CSK_GPDMA_EVENT_TRANSFER_DONE) {
        lisa_semaphore_give(priv->dma_sem);
    }
}

static int arcs_qspilcd_transfer(lisa_device_t *dev, const lisa_qspilcd_xfer_t *xfer)
{
    if (!dev || !dev->priv_data || !xfer || !xfer->buf || xfer->size_bytes == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_qspilcd_priv_t *priv = (lisa_qspilcd_priv_t *)dev->priv_data;

    QSPILCD_LOCK(priv);

    uint32_t ret = QSPI_LCD_SetDatLength(priv->hal_handler, xfer->data_bits);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "QSPI_LCD_SetDatLength failed (%d)", ret);
        goto err;
    }

    em_CSK_QSPI_LCD_LANE_NUM lane_num = to_hal_lane(xfer->lane);
    ret = QSPI_LCD_SetLaneNum(priv->hal_handler, lane_num);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "QSPI_LCD_SetLaneNum failed (%d)", ret);
        goto err;
    }

    ret = QSPI_LCD_DMADisable(priv->hal_handler);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "QSPI_LCD_DMADisable failed (%d)", ret);
        goto err;
    }

    if (!xfer->use_dma) {
        ret = QSPI_LCD_Control(priv->hal_handler, CSK_QSPI_LCD_RESET_FIFO, 0);
        if (ret != CSK_DRIVER_OK) {
            LISA_LOGE(LOG_TAG, "QSPI_LCD_Control failed (%d)", ret);
            goto err;
        }
        ret = QSPI_LCD_Send(priv->hal_handler, xfer->buf, xfer->size_bytes);
        if (ret != CSK_DRIVER_OK) {
            LISA_LOGE(LOG_TAG, "QSPI_LCD_Send failed (%d)", ret);
            goto err;
        }
        QSPI_LCD_Wait_Done(priv->hal_handler);
    } else {
        ret = QSPI_LCD_SetDMASize(priv->hal_handler, xfer->size_bytes);
        if (ret != CSK_DRIVER_OK) {
            LISA_LOGE(LOG_TAG, "QSPI_LCD_SetDMASize failed (%d)", ret);
            goto err;
        }

        ret = QSPI_LCD_DMAEnable(priv->hal_handler);
        if (ret != CSK_DRIVER_OK) {
            LISA_LOGE(LOG_TAG, "QSPI_LCD_DMAEnable failed (%d)", ret);
            goto err;
        }

        ret = GPDMA_Start_Normal(CONFIG_LISA_QSPILCD_GPDMA_CH, (void *)xfer->buf,
                                                (void *)QSPI_LCD_Buf(), xfer->size_bytes/2);
        if (ret != CSK_DRIVER_OK) {
            LISA_LOGE(LOG_TAG, "GPDMA_Start_Normal failed (%d)", ret);
            goto err;
        }
    }

    QSPILCD_UNLOCK(priv);
    return LISA_DEVICE_OK;
err:
    QSPILCD_UNLOCK(priv);
    return LISA_DEVICE_ERR_IO;
}

static int arcs_qspilcd_wait_done(lisa_device_t *dev, uint32_t timeout_ms)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_qspilcd_priv_t *priv = (lisa_qspilcd_priv_t *)dev->priv_data;
    if (!priv->hal_handler) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (lisa_semaphore_take(priv->dma_sem, timeout_ms) != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "lisa_semaphore_take failed");
        return LISA_DEVICE_ERR_TIMEOUT;
    }
    return LISA_DEVICE_OK;
}

static int arcs_qspilcd_control(lisa_device_t *dev, uint32_t control, uint32_t arg)
{
    /* 完善的参数验证 */
    if (!lisa_device_is_initialized(dev) || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_qspilcd_priv_t *priv = (lisa_qspilcd_priv_t *)dev->priv_data;

    /* 检查 HAL 句柄是否已准备就绪 */
    if (priv->hal_handler == NULL) {
        LISA_LOGE(LOG_TAG, "HAL handler not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 使用锁保护控制操作 */
    QSPILCD_LOCK(priv);

    int32_t hal_ret = QSPI_LCD_Control(priv->hal_handler, control, arg);

    QSPILCD_UNLOCK(priv);

    uint32_t excl_op = control & LISA_QSPILCD_EXCL_OP_Msk;
    if (excl_op == LISA_QSPILCD_GET_BUS_SPEED) {
        return (hal_ret < 0) ? hal_status_to_err(hal_ret) : (int)hal_ret;
    }

    return hal_status_to_err(hal_ret);
}

int arcs_qspilcd_set_lane(lisa_device_t *dev, lisa_qspilcd_lane_num_t lane)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_qspilcd_priv_t *priv = (lisa_qspilcd_priv_t *)dev->priv_data;
    if (!priv->hal_handler) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    QSPILCD_LOCK(priv);

    em_CSK_QSPI_LCD_LANE_NUM lane_num = to_hal_lane(lane);
    // 调用 HAL 层设置数据线数量
    uint32_t ret = QSPI_LCD_SetLaneNum(priv->hal_handler, lane_num);

    QSPILCD_UNLOCK(priv);
    return (ret == CSK_DRIVER_OK) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_IO;
}

int arcs_qspilcd_set_data_bits(lisa_device_t *dev, uint8_t data_bits)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_qspilcd_priv_t *priv = (lisa_qspilcd_priv_t *)dev->priv_data;
    if (!priv->hal_handler) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    QSPILCD_LOCK(priv);

    // 调用 HAL 层设置数据位宽
    uint32_t ret = QSPI_LCD_SetDatLength(priv->hal_handler, data_bits);

    QSPILCD_UNLOCK(priv);
    return (ret == CSK_DRIVER_OK) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_IO;
}

static void arcs_qspilcd_cs_control(lisa_device_t *dev, bool level)
{
    if (!dev || !dev->priv_data) {
        return;
    }

    lisa_qspilcd_priv_t *priv = (lisa_qspilcd_priv_t *)dev->priv_data;
    if (!priv->cs_gpio) {
        return;
    }

    QSPILCD_LOCK(priv);
    lisa_gpio_write_pin(priv->cs_gpio, priv->cs_pin, level ? 1 : 0);
    QSPILCD_UNLOCK(priv);
}

static int arcs_qspilcd_cs_configure(lisa_device_t *dev, lisa_device_t *gpio_dev, uint32_t cs_pin)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_qspilcd_priv_t *priv = (lisa_qspilcd_priv_t *)dev->priv_data;

    QSPILCD_LOCK(priv);

    priv->cs_gpio = gpio_dev;
    priv->cs_pin = cs_pin;

    LISA_LOGI(LOG_TAG, "CS GPIO configured: gpio=%p, pin=%lu",
              (void *)gpio_dev, cs_pin);

    // 如果 GPIO 设备已准备好，立即配置 CS 引脚
    if (gpio_dev && lisa_device_ready(gpio_dev)) {
        int ret = lisa_gpio_configure(gpio_dev, cs_pin, LISA_GPIO_CONFIG_OUTPUT_HIGH);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "Failed to configure CS GPIO: %d", ret);
            QSPILCD_UNLOCK(priv);
            return LISA_DEVICE_ERR_INIT_FAIL;
        }
        LISA_LOGI(LOG_TAG, "CS GPIO pin configured successfully");
    }

    QSPILCD_UNLOCK(priv);
    return LISA_DEVICE_OK;
}

static const lisa_qspilcd_api_t arcs_qspilcd_api = {
    .control        = arcs_qspilcd_control,
    .set_lane       = arcs_qspilcd_set_lane,
    .transfer       = arcs_qspilcd_transfer,
    .wait_done      = arcs_qspilcd_wait_done,
    .cs_control     = arcs_qspilcd_cs_control,
    .cs_configure   = arcs_qspilcd_cs_configure,
    .set_data_bits  = arcs_qspilcd_set_data_bits,
};

static lisa_qspilcd_priv_t qspilcd_priv;

static int arcs_qspilcd_init(void)
{
    uint32_t ret = 0;

    memset(&qspilcd_priv, 0, sizeof(qspilcd_priv));

    qspilcd_priv.lock = lisa_mutex_create();
    if (!qspilcd_priv.lock) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    qspilcd_priv.dma_sem = lisa_semaphore_create(1);
    if (!qspilcd_priv.dma_sem) {
        LISA_LOGE(LOG_TAG, "Failed to create DMA semaphore");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    if (qspilcd_priv.cs_gpio && lisa_device_ready(qspilcd_priv.cs_gpio)) {
        // Configure CS GPIO
        ret = lisa_gpio_configure(
            qspilcd_priv.cs_gpio, qspilcd_priv.cs_pin, LISA_GPIO_CONFIG_OUTPUT_HIGH);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(LOG_TAG, "Failed to configure CS GPIO: %d", ret);
            return LISA_DEVICE_ERR_INIT_FAIL;
        }
    }
    qspilcd_priv.hal_handler = QSPI_LCD();

    csk_gpdma_init_t gpdma_cfg = {
        .dma_ch       = CONFIG_LISA_QSPILCD_GPDMA_CH,
        .src_mode     = address_mode_normal,
        .dst_mode     = address_mode_normal,
        .tfr_mode     = tfr_mode_m2p,
        .prio_lvl     = prio_mode_vhigh,
        .handshake    = qspi_hs_num0,
        .burst_len    = gpdma_burst_len_2spl,
        .sample_unit  = gpdma_sample_unit_halfword,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
    };

    ret = GPDMA_Initialize();
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "Failed to initialize GPDMA: %d", ret);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    ret = GPDMA_Config(&gpdma_cfg, qspilcd_dma_callback, &qspilcd_priv);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "Failed to config GPDMA: %d", ret);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    ret = QSPI_LCD_Initialize(QSPI_LCD(), NULL, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to initialize QSPI LCD: %d", ret);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    ret = QSPI_LCD_PowerControl(QSPI_LCD(), CSK_POWER_FULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to set QSPI LCD power state: %d", ret);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    ret = QSPI_LCD_SetLcdMode(QSPI_LCD(), CSK_QSPI_LCD_MODE_NORMAL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to set QSPI LCD mode: %d", ret);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    lisa_qspi_lcd_pinmux();
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(qspilcd0, &arcs_qspilcd_api, &qspilcd_priv, NULL, arcs_qspilcd_init, LISA_DEVICE_PRIORITY_NORMAL);
