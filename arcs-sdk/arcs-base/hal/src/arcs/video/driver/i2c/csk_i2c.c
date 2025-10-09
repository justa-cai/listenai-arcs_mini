#include "csk_i2c.h"


static void* I2C_Handle[2] = {NULL};
static volatile uint32_t I2C_Event[2] = {0};

static inline void CLR_XFER_DONE(uint8_t i2c_index)
{
    I2C_Event[i2c_index] = 0;
}

static inline bool GET_XFER_DONE(uint8_t i2c_index)
{
    return (I2C_Event[i2c_index] & CSK_I2C_EVENT_TRANSFER_DONE);
}


static bool wait_xfer_done_timeout(uint8_t i2c_index, uint32_t max_wait_ms)
{
    bool ret = false;
    uint32_t cnt = 0;

    while(cnt++ < (max_wait_ms * 1000))
    {
        if(GET_XFER_DONE(i2c_index))
        {
            CLR_XFER_DONE(i2c_index);
            ret = true;
            break;
        }
        DELAY_US(1);
    }
    CLR_XFER_DONE(i2c_index);

    return ret;
}


static void I2C0_EventCallback(uint32_t event, void* workspace){
    I2C_Event[0] |= event;
}

static void I2C1_EventCallback(uint32_t event, void* workspace){
    I2C_Event[1] |= event;
}


/**
  * @brief  Initializes Camera low level.
  * @retval None
  */
int csk_i2c_init(uint8_t i2c_index)
{
    if (i2c_index > 1) {
        VIDEO_LOG("[%s:%d] i2c_index=%d error, must be 0/1", __func__, __LINE__, i2c_index);
        return -1;
    }

//    IOMuxManager_PinConfigure(USER_I2C_IOMUX_PAD, USER_I2C_SCL_PIN, USER_I2C_IOMUX_FUN);
//    IOMuxManager_PinConfigure(USER_I2C_IOMUX_PAD, USER_I2C_SDA_PIN, USER_I2C_IOMUX_FUN);

    if (i2c_index == 0) {
        I2C_Handle[i2c_index] = I2C0();
        I2C_Initialize(I2C_Handle[i2c_index], I2C0_EventCallback, NULL);
    } else {
        I2C_Handle[i2c_index] = I2C1();
        I2C_Initialize(I2C_Handle[i2c_index], I2C1_EventCallback, NULL);
    }

    I2C_PowerControl(I2C_Handle[i2c_index], CSK_POWER_FULL);

    /* CSK_I2C_TRANSMIT_MODE:  arg0 = 1, means DMA mode ;  arg0 = 0, means Interrupt mode */
    I2C_Control(I2C_Handle[i2c_index], CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_Handle[i2c_index], CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);  // CSK_I2C_BUS_SPEED_STANDARD/CSK_I2C_BUS_SPEED_FAST
    I2C_Control(I2C_Handle[i2c_index], CSK_I2C_BUS_CLEAR, 0);

    CLR_XFER_DONE(i2c_index);

    return 0;
}


/**
  * @brief  Camera writes single data.
  * @param  Addr: I2C address
  * @param  Reg: Register address
  * @param  Value: Data to be written
  * @retval None
  */
int csk_i2c_write_reg8(uint8_t i2c_index, uint16_t addr, uint8_t reg, uint8_t value)
{
    int32_t ret = 0;
    uint8_t i2c_data[2];

    if (i2c_index > 1) {
        VIDEO_LOG("[%s:%d] i2c_index=%d error, must be 0/1", __func__, __LINE__, i2c_index);
        return -1;
    }

    if (I2C_Handle[i2c_index] == NULL) {
        VIDEO_LOG("[%s:%d] i2c_index=%d handle is NULL, not init", __func__, __LINE__, i2c_index);
        return -1;
    }

    CLR_XFER_DONE(i2c_index);

    i2c_data[0] = reg;
    i2c_data[1] = value;
    ret = I2C_MasterTransmit(I2C_Handle[i2c_index], addr, i2c_data, 2, 0);
    if (CSK_DRIVER_OK != ret) {
        VIDEO_LOG("[%s:%d] I2C error ret=%d", __func__, __LINE__, ret);
        return -1;
    }
    if (!wait_xfer_done_timeout(i2c_index, 1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        return -1;
    }

    return 0;
}


/**
  * @brief  Camera writes single data.
  * @param  Addr: I2C address
  * @param  Reg: Register address
  * @param  Value: Data to be written
  * @retval None
  */
int csk_i2c_write_reg16(uint8_t i2c_index, uint16_t addr, uint16_t reg, uint8_t value)
{
    int32_t ret = 0;
    uint8_t i2c_data[3];

    if (i2c_index > 1) {
        VIDEO_LOG("[%s:%d] i2c_index=%d error, must be 0/1", __func__, __LINE__, i2c_index);
        return -1;
    }

    if (I2C_Handle[i2c_index] == NULL) {
        VIDEO_LOG("[%s:%d] i2c_index=%d handle is NULL, not init", __func__, __LINE__, i2c_index);
        return -1;
    }

    CLR_XFER_DONE(i2c_index);

    i2c_data[0] = (reg >> 8) & 0xff;
    i2c_data[1] = reg & 0xff;
    i2c_data[2] = value;
    ret = I2C_MasterTransmit(I2C_Handle[i2c_index], addr, i2c_data, 3, 0);
    if (CSK_DRIVER_OK != ret) {
        VIDEO_LOG("[%s:%d] I2C error ret=%d", __func__, __LINE__, ret);
        return -1;
    }
    if (!wait_xfer_done_timeout(i2c_index, 1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        return -1;
    }

    return 0;
}


/**
  * @brief  Camera reads single data.
  * @param  Addr: I2C address
  * @param  Reg: Register address
  * @retval Read data number
  */
int csk_i2c_read(uint8_t i2c_index, uint16_t addr, uint8_t reg, uint8_t *data, uint16_t num)
{
    int32_t ret = 0;

    if (i2c_index > 1) {
        VIDEO_LOG("[%s:%d] i2c_index=%d error, must be 0/1", __func__, __LINE__, i2c_index);
        return -1;
    }

    if (I2C_Handle[i2c_index] == NULL) {
        VIDEO_LOG("[%s:%d] i2c_index=%d handle is NULL, not init", __func__, __LINE__, i2c_index);
        return -1;
    }

    CLR_XFER_DONE(i2c_index);

    ret = I2C_MasterTransmit(I2C_Handle[i2c_index], addr, &reg, 1, 1);
    if (CSK_DRIVER_OK != ret) {
        VIDEO_LOG("[%s:%d] I2C error ret=%d", __func__, __LINE__, ret);
        return -1;
    }
    if (!wait_xfer_done_timeout(i2c_index, 1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        return -1;
    }

    ret = I2C_MasterReceive(I2C_Handle[i2c_index], addr, data, num, 0);
    if (CSK_DRIVER_OK != ret) {
        VIDEO_LOG("[%s:%d] I2C error ret=%d", __func__, __LINE__, ret);
        return -1;
    }
    if (!wait_xfer_done_timeout(i2c_index, 1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        return -1;
    }

    return num;
}


/**
  * @brief  Camera reads single data.
  * @param  Addr: I2C address
  * @param  Reg: Register address
  * @retval Read data number
  */
uint8_t csk_i2c_read_reg8(uint8_t i2c_index, uint16_t addr, uint8_t reg)
{
    uint8_t data;

    if(csk_i2c_read(i2c_index, addr, reg, &data, 1)) {
        return data;
    } else {
        return 0;
    }
}


/**
  * @brief  Camera reads single data.
  * @param  Addr: I2C address
  * @param  Reg: Register address
  * @retval Read data number
  */
uint8_t csk_i2c_read_reg16(uint8_t i2c_index, uint16_t addr, uint16_t reg)
{
    int32_t ret = 0;
    uint8_t data[2];

    if (i2c_index > 1) {
        VIDEO_LOG("[%s:%d] i2c_index=%d error, must be 0/1", __func__, __LINE__, i2c_index);
        return -1;
    }

    if (I2C_Handle[i2c_index] == NULL) {
        VIDEO_LOG("[%s:%d] i2c_index=%d handle is NULL, not init", __func__, __LINE__, i2c_index);
        return -1;
    }

    CLR_XFER_DONE(i2c_index);

    data[0] = (reg >> 8) & 0xff;
    data[1] = reg & 0xff;
    ret = I2C_MasterTransmit(I2C_Handle[i2c_index], addr, &data[0], 2, 1);
    if (CSK_DRIVER_OK != ret) {
        VIDEO_LOG("[%s:%d] I2C error ret=%d", __func__, __LINE__, ret);
        return -1;
    }
    if (!wait_xfer_done_timeout(i2c_index, 1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        return -1;
    }

    ret = I2C_MasterReceive(I2C_Handle[i2c_index], addr, &data[0], 1, 0);
    if (CSK_DRIVER_OK != ret) {
        VIDEO_LOG("[%s:%d] I2C error ret=%d", __func__, __LINE__, ret);
        return -1;
    }
    if (!wait_xfer_done_timeout(i2c_index, 1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        return -1;
    }

    return data[0];
}


int csk_i2c_detect(uint8_t i2c_index)
{
    int32_t ret = 0;
    uint16_t addr = 0;
    uint8_t cnt = 0;

    if (i2c_index > 1) {
        VIDEO_LOG("[%s:%d] i2c_index=%d error, must be 0/1", __func__, __LINE__, i2c_index);
        return -1;
    }

    for(addr = 0; addr <=0x7F; addr++)
    {
        csk_i2c_init(i2c_index);
        CLR_XFER_DONE(i2c_index);

        //VIDEO_LOG("[%s:%d] addr=0x%x", __func__, __LINE__, addr);

        ret = I2C_MasterReceive(I2C_Handle[i2c_index], addr, 0x00, 1, 0);
        //VIDEO_LOG("[%s:%d] ret=%d", __func__, __LINE__, ret);
        if (!wait_xfer_done_timeout(i2c_index, 1)) { // 1ms
            //VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        }

        ret = I2C_MasterReceive(I2C_Handle[i2c_index], addr, 0x00, 1, 0);
        //VIDEO_LOG("[%s:%d] ret=%d", __func__, __LINE__, ret);
        if (!wait_xfer_done_timeout(i2c_index, 1)) { // 1ms
            //VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        }

        if(CSK_DRIVER_OK == ret) {
            VIDEO_LOG("[%s:%d] get i2c slave device addr_7bit=0x%x", __func__, __LINE__, addr);
            cnt++;
        }
    }

    return cnt;
}


