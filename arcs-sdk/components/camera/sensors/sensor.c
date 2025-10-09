#include <stdbool.h>
#include <stdint.h>
#include "arcs_ap.h"
#include "log_print.h"
#include "sensor.h"
#include "Driver_Common.h"
#include "Driver_I2C.h"
#include "IOMuxManager.h"
#include "lisa_log.h"
#ifdef CONFIG_CORE_SPINLOCK
#include "core_spinlock.h"
#endif

#if CONFIG_CORE_SPINLOCK
#define SPI_SENSOR_TWI_PIN_SDA       (0)
#define SPI_SENSOR_TWI_PIN_SCL       (1)
#else
#define SPI_SENSOR_TWI_PIN_SDA       (0)
#define SPI_SENSOR_TWI_PIN_SCL       (1)
#endif

#if CONFIG_CORE_SPINLOCK
#define IRQ_I2C_VECTOR              IRQ_I2C1_VECTOR
#else
#define IRQ_I2C_VECTOR              IRQ_I2C0_VECTOR
#endif

#define I2C_EVT_IDLE  XBIT(0)

#ifdef CONFIG_CORE_SPINLOCK
SPINLOCK_DEFINE(i2c_spinlock);
#endif

static void *i2c_dev = NULL;

const camera_sensor_info_t camera_sensor[CAMERA_MODEL_MAX] = {
    {CAMERA_OV7725, "OV7725", OV7725_SCCB_ADDR, OV7725_PID, FRAMESIZE_VGA, false},
    {CAMERA_OV2640, "OV2640", OV2640_SCCB_ADDR, OV2640_PID, FRAMESIZE_UXGA, true},
    {CAMERA_OV3660, "OV3660", OV3660_SCCB_ADDR, OV3660_PID, FRAMESIZE_QXGA, true},
    {CAMERA_OV5640, "OV5640", OV5640_SCCB_ADDR, OV5640_PID, FRAMESIZE_QSXGA, true},
    {CAMERA_OV7670, "OV7670", OV7670_SCCB_ADDR, OV7670_PID, FRAMESIZE_VGA, false},
    {CAMERA_NT99141, "NT99141", NT99141_SCCB_ADDR, NT99141_PID, FRAMESIZE_HD, true},
    {CAMERA_GC2145, "GC2145", GC2145_SCCB_ADDR, GC2145_PID, FRAMESIZE_UXGA, false},
    {CAMERA_GC032A, "GC032A", GC032A_SCCB_ADDR, GC032A_PID, FRAMESIZE_VGA, false},
    {CAMERA_GC0328, "GC0328", GC0328_SCCB_ADDR, GC0328_PID, FRAMESIZE_VGA, false},
    {CAMERA_GC0310, "GC0310", GC0310_SCCB_ADDR, GC0310_PID, FRAMESIZE_VGA, false},
    {CAMERA_GC0308, "GC0308", GC0308_SCCB_ADDR, GC0308_PID, FRAMESIZE_VGA, false},
    {CAMERA_BF3005, "BF3005", BF3005_SCCB_ADDR, BF3005_PID, FRAMESIZE_VGA, false},
    {CAMERA_BF20A6, "BF20A6", BF20A6_SCCB_ADDR, BF20A6_PID, FRAMESIZE_VGA, false},
    {CAMERA_BF3901, "BF3901", BF3901_SCCB_ADDR, BF3901_PID, FRAMESIZE_QVGA, false},
    {CAMERA_SC101IOT, "SC101IOT", SC101IOT_SCCB_ADDR, SC101IOT_PID, FRAMESIZE_HD, false},
    {CAMERA_SC030IOT, "SC030IOT", SC030IOT_SCCB_ADDR, SC030IOT_PID, FRAMESIZE_VGA, false},
    {CAMERA_SC031GS, "SC031GS", SC031GS_SCCB_ADDR, SC031GS_PID, FRAMESIZE_VGA, false},
    {CAMERA_OV9655, "OV9655", OV9655_SCCB_ADDR, OV9655_PID, FRAMESIZE_VGA, false},
};

const resolution_info_t resolution[FRAMESIZE_INVALID] = {
    {   96,   96, ASPECT_RATIO_1X1   }, /* 96x96 */
    {  128,  180, ASPECT_RATIO_1X1   }, /* 128x180 */
    {  160,  120, ASPECT_RATIO_4X3   }, /* QQVGA */
    {  176,  144, ASPECT_RATIO_5X4   }, /* QCIF  */
    {  240,  176, ASPECT_RATIO_4X3   }, /* HQVGA */
    {  240,  240, ASPECT_RATIO_1X1   }, /* 240x240 */
    {  320,  240, ASPECT_RATIO_4X3   }, /* QVGA  */
    {  400,  296, ASPECT_RATIO_4X3   }, /* CIF   */
    {  480,  320, ASPECT_RATIO_3X2   }, /* HVGA  */
    {  640,  480, ASPECT_RATIO_4X3   }, /* VGA   */
    {  800,  600, ASPECT_RATIO_4X3   }, /* SVGA  */
    { 1024,  768, ASPECT_RATIO_4X3   }, /* XGA   */
    { 1280,  720, ASPECT_RATIO_16X9  }, /* HD    */
    { 1280, 1024, ASPECT_RATIO_5X4   }, /* SXGA  */
    { 1600, 1200, ASPECT_RATIO_4X3   }, /* UXGA  */
    // 3MP Sensors
    { 1920, 1080, ASPECT_RATIO_16X9  }, /* FHD   */
    {  720, 1280, ASPECT_RATIO_9X16  }, /* Portrait HD   */
    {  864, 1536, ASPECT_RATIO_9X16  }, /* Portrait 3MP   */
    { 2048, 1536, ASPECT_RATIO_4X3   }, /* QXGA  */
    // 5MP Sensors
    { 2560, 1440, ASPECT_RATIO_16X9  }, /* QHD    */
    { 2560, 1600, ASPECT_RATIO_16X10 }, /* WQXGA  */
    { 1088, 1920, ASPECT_RATIO_9X16  }, /* Portrait FHD   */
    { 2560, 1920, ASPECT_RATIO_4X3   }, /* QSXGA  */
};


// struct spi_sensor_twi {
//     void *twi_port;
//     uint8_t slave_addr;
// };

// struct spi_sensor_twi sensor_twi_cfg;

static volatile uint32_t i2c_event = 0;

camera_sensor_info_t *camera_sensor_get_info(camera_model_t model)
{
    for (int i = 0; i < CAMERA_MODEL_MAX; i++) {
        if (camera_sensor[i].model == model) {
            return (camera_sensor_info_t *)&camera_sensor[i];
        }
    }
    return NULL;
}

static void twi_m_event_cb(uint32_t event, void *workspace)
{
    // LOGI("twi event: %x\n", event);
    i2c_event |= event;
}

void sensor_twi_lock_init(void)
{
    #ifdef CONFIG_CORE_SPINLOCK
    spinlock_init((uint32_t *)&i2c_spinlock);
    #endif
}

int32_t sensor_twi_init(const i2c_config_t *config)
{
    int ret = 0;

    #ifdef CONFIG_CORE_SPINLOCK
    memset((void *)&i2c_spinlock, 0x0, sizeof(i2c_spinlock));
    #endif

    i2c_dev = config->i2c_dev;
    IOMuxManager_PinConfigure(config->pins.sda.pad, config->pins.sda.pin, config->pins.sda.func);
    IOMuxManager_PinConfigure(config->pins.scl.pad, config->pins.scl.pin, config->pins.scl.func);

    #ifdef CONFIG_CORE_SPINLOCK
    spinlock_acquire(&i2c_spinlock);
    #endif
    ret = I2C_Initialize(config->i2c_dev, twi_m_event_cb, NULL);
    if (ret != CSK_DRIVER_OK) {
        LOGE("I2C_Initialize failed %d", ret);
        goto end;
    }
    ret = I2C_PowerControl(config->i2c_dev, CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK) {
        LOGE("I2C_PowerControl failed %d", ret);
        goto end;
    }
    ret = I2C_Control(config->i2c_dev, CSK_I2C_TRANSMIT_MODE, 0);
    if (ret != CSK_DRIVER_OK) {
        LOGE("I2C_Control CSK_I2C_TRANSMIT_MODE failed %d", ret);
        goto end;
    }
    ret = I2C_Control(config->i2c_dev, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_FAST);
    if (ret != CSK_DRIVER_OK) {
        LOGE("I2C_Control CSK_I2C_BUS_SPEED failed %d", ret);
        goto end;
    }
    I2C_Control(config->i2c_dev, CSK_I2C_BUS_CLEAR, 0);
    if (ret != CSK_DRIVER_OK) {
        LOGE("I2C_Control CSK_I2C_BUS_CLEAR failed %d", ret);
        goto end;
    }

end:
    #ifdef CONFIG_CORE_SPINLOCK
    disable_IRQ(IRQ_I2C_VECTOR);
    spinlock_release(&i2c_spinlock);
    #endif
    return ret;
}

int32_t sensor_twi_exit(void)
{
    int32_t ret = 0;

    I2C_PowerControl(i2c_dev, CSK_POWER_OFF);

    ret = I2C_Uninitialize(i2c_dev);

    return ret;
}

static int _twi_write_raw8(uint8_t slvaddr, uint8_t *values, int count, bool xfstop)
{
    i2c_event &= ~CSK_I2C_EVENT_TRANSFER_DONE;
#ifdef CONFIG_CORE_SPINLOCK
    spinlock_acquire(&i2c_spinlock);
    enable_IRQ(IRQ_I2C_VECTOR);
    I2C_Control(i2c_dev, CSK_I2C_BUS_CLEAR, 0);
#endif

    int ret = I2C_MasterTransmit(i2c_dev, slvaddr, values, count, xfstop ? 0 : 1);
    if (ret == CSK_DRIVER_OK) {
        while ((i2c_event & CSK_I2C_EVENT_TRANSFER_DONE) == 0);
    }

#ifdef CONFIG_CORE_SPINLOCK
    disable_IRQ(IRQ_I2C_VECTOR);
    spinlock_release(&i2c_spinlock);
#endif

    return ret;
}

static int _twi_write(uint8_t slv_addr, uint16_t reg, unsigned char value, bool reg16_width)
{
    int32_t ret = 0;
    uint8_t buf[3];
    uint32_t num = 0;

    if (reg16_width) {
        buf[0] = ((reg >> 8) & 0xff);
        buf[1] = (reg & 0xff);
        buf[2] = value;
        num = 3;
    }
    else {
        buf[0] = reg;
        buf[1] = value;
        num = 2;
    }


    i2c_event &= ~CSK_I2C_EVENT_TRANSFER_DONE;
    #ifdef CONFIG_CORE_SPINLOCK
    spinlock_acquire(&i2c_spinlock);
    enable_IRQ(IRQ_I2C_VECTOR);
    I2C_Control(i2c_dev, CSK_I2C_BUS_CLEAR, 0);
    #endif

    // taskENTER_CRITICAL();
    ret = I2C_MasterTransmit(i2c_dev, slv_addr, buf, num, false);
    if (ret != CSK_DRIVER_OK) {
        // taskEXIT_CRITICAL();
        goto end;
    }
    // taskEXIT_CRITICAL();

    while ((i2c_event & CSK_I2C_EVENT_TRANSFER_DONE) == 0) {}

end:
    #ifdef CONFIG_CORE_SPINLOCK
    disable_IRQ(IRQ_I2C_VECTOR);
    spinlock_release(&i2c_spinlock);
    #endif

    return ret;
}

static int _twi_read(uint8_t slv_addr, uint16_t reg, unsigned char *value, bool reg16_width)
{
    int ret = 0;
    int num = 0;

    num = reg16_width ? 2 : 1;

    // taskENTER_CRITICAL();
    i2c_event &= ~CSK_I2C_EVENT_TRANSFER_DONE;
    #ifdef CONFIG_CORE_SPINLOCK
    spinlock_acquire(&i2c_spinlock);
    enable_IRQ(IRQ_I2C_VECTOR);
    I2C_Control(i2c_dev, CSK_I2C_BUS_CLEAR, 0);
    #endif

    ret = I2C_MasterTransmit(i2c_dev, slv_addr, (const uint8_t *)&reg, num, true);
    if (ret != CSK_DRIVER_OK) {
        CLOGE("i2c master transmit failed %d\r\n", ret);
        goto end;
    }

    while ((i2c_event & CSK_I2C_EVENT_TRANSFER_DONE) == 0) {}

    i2c_event &= ~CSK_I2C_EVENT_TRANSFER_DONE;
    ret = I2C_MasterReceive(i2c_dev, slv_addr, value, 1, false);
    if (ret != CSK_DRIVER_OK) {
        CLOGE("i2c master receive failed %d\r\n", ret);
        goto end;
    }

    while ((i2c_event & CSK_I2C_EVENT_TRANSFER_DONE) == 0) {}

    // taskEXIT_CRITICAL();

end:
    #ifdef CONFIG_CORE_SPINLOCK
    disable_IRQ(IRQ_I2C_VECTOR);
    spinlock_release(&i2c_spinlock);
    #endif
    // CLOGI("read val: %x\r\n", *value);
    return ret;
}

uint8_t sensor_twi_read_reg8(uint8_t slv_addr, uint8_t reg)
{
    int ret = 0;
    uint8_t value = 0;
    int cnt = 0;

    ret = _twi_read(slv_addr, reg, &value, false);
    while (ret != 0 && cnt < 2) {
        CLOGW("twi read retry %d\r\n", cnt);
        ret = _twi_read(slv_addr, reg, &value, false);
        // log....
        cnt++;
    }
    // if (cnt > 0)
        // log...

    return value;
}

uint8_t sensor_twi_read_reg16(uint8_t slv_addr, uint16_t reg)
{
    int ret = 0;
    uint8_t value = 0;
    int cnt = 0;

    ret = _twi_read(slv_addr, reg, &value, true);
    while (ret != 0 && cnt < 2) {
        CLOGW("twi read retry %d\r\n", cnt);
        ret = _twi_read(slv_addr, reg, &value, true);
        // log....
        cnt++;
    }
    // if (cnt > 0)
        // log...

    return value;
}

int sensor_twi_write_reg8(uint8_t slv_addr, uint8_t reg, uint8_t value)
{
    int ret = 0;
    int cnt = 0;

    ret = _twi_write(slv_addr, reg, value, false);
    while (ret != 0 && cnt < 2) {
        ret = _twi_write(slv_addr, reg, value, false);
        cnt++;
        CLOGW("twi write retry %d %d\r\n", cnt, ret);
    }

    return ret;
}

int sensor_twi_write_reg16(uint8_t slv_addr, uint16_t reg, uint8_t value)
{
    int ret = 0;
    int cnt = 0;

    ret = _twi_write(slv_addr, reg, value, true);
    while (ret != 0 && cnt < 2) {
        ret = _twi_write(slv_addr, reg, value, true);
        cnt++;
        CLOGW("twi write retry %d %d\r\n", cnt, ret);
    }

    return ret;
}

int sensor_twi_write_raw8(uint8_t slvaddr, uint8_t *values, int count)
{
    return _twi_write_raw8(slvaddr, values, count, true);
}

// int sensor_twi_write_array(uint8_t slv_addr, struct regval_list *regs, int array_size)
// {
//     int ret = 0;
//     int i = 0;

//     if (!regs)
//         return -1;

//     while (i < array_size) {
//         if (regs->addr == REG_DLY) {
//             // hal_msleep(regs->data);
//         }
//         else {
//             ret = sensor_twi_write_reg8(slv_addr, regs->addr, regs->data);
//             if (ret != 0) {
//                 // Log....
//                 CLOGE("spi_sensor_twi_write failed %d\r\n", ret);
//                 return ret;
//             }
//         }
//         i++;
//         regs++;
//     }
//     return ret;
// }
