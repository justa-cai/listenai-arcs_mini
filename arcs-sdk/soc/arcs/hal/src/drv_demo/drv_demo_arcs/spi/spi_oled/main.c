/*
 * main.c
 *
 *  Created on: Apr 13, 2020 (for castor CSK4002)
 *  Ported on: Sept. 1, 2020 (for venus CSK6001)
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//#include "venus_ap.h"
//#include "venus_log.h"
#include "arcs_ap.h"
#include "log_print.h"
#include "nos_timer.h"

//#include "FreeRtos.h"
//#include "task.h"
//#include "semphr.h"

#include "Driver_SPI.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "ClockManager.h" // for BootClock_Init etc.

//oled screen size
#define Max_Column 240
#define Max_Row 240

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Global Variables
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//#define	SPI_Send 		SPI_Send_NEnd
#define	SPI_Send(...)	SPI_Send_PIO_Lite(__VA_ARGS__, 1)
//#define	SPI_Send(dev, dout, ...)	SPI_Transfer_PIO_Lite(dev, dout, Block2, __VA_ARGS__, 1)

#define SPI_INDEX               2 //0 //1 //

//#define SPI_TRANSFER_TIMEOUT    3
#define SPI_DATA_BITS           8

//SPI1 GPIO Configuration
#if (IC_BOARD == 0)
#define SPI_BUS_SPEED           2000000 // 2MHz
#else
#define SPI_BUS_SPEED           10000000 // 60000000 // 10MHz, 15MHz, 30MHz, 60MHz
//#error Redefine PINs of SPI OLED on ASIC!! // FIXME:
#endif // IC_BOARD

// it MAY BE CHANGED, see ARCS_PIN_Mapping.xlsx...
#if (SPI_INDEX == 0)
#define SPI0_CLK_PIN          CSK_IOMUX_PAD_A, 15, CSK_IOMUX_FUNC_ALTER5
#define SPI0_CS_PIN           CSK_IOMUX_PAD_A, 6, CSK_IOMUX_FUNC_ALTER5
//#define SPI0_MISO_PIN         CSK_IOMUX_PAD_A, 13, CSK_IOMUX_FUNC_ALTER5
#define SPI0_MOSI_PIN         CSK_IOMUX_PAD_A, 14, CSK_IOMUX_FUNC_ALTER5

#elif (SPI_INDEX == 1)
#define SPI1_CLK_PIN          CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER6
#define SPI1_CS_PIN           CSK_IOMUX_PAD_A, 7, CSK_IOMUX_FUNC_ALTER6
//#define SPI1_MISO_PIN         CSK_IOMUX_PAD_B, 6, CSK_IOMUX_FUNC_ALTER6
#define SPI1_MOSI_PIN         CSK_IOMUX_PAD_B, 7, CSK_IOMUX_FUNC_ALTER6

#elif (SPI_INDEX == 2)
#define SPI2_CLK_PIN          CSK_IOMUX_PAD_B, 0, CSK_IOMUX_FUNC_ALTER7
#define SPI2_CS_PIN           CSK_IOMUX_PAD_B, 1, CSK_IOMUX_FUNC_ALTER7
//#define SPI2_MISO_PIN         CSK_IOMUX_PAD_A, 30, CSK_IOMUX_FUNC_ALTER7
#define SPI2_MOSI_PIN         CSK_IOMUX_PAD_A, 31, CSK_IOMUX_FUNC_ALTER7
#endif


#define RESET_PIN_NUM       16 //FIXME:
#define DC_PIN_NUM          17 //FIXME:
#define BLK_PIN_NUM         18 //FIXME:

#define RESET_PIN       CSK_IOMUX_PAD_A, RESET_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT
#define DC_PIN          CSK_IOMUX_PAD_A, DC_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT
#define BLK_PIN         CSK_IOMUX_PAD_A, BLK_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT

static void *gGpioDev = NULL;
static void *gSpiDev = NULL;

// in the non-cacheable RAM ?
_DMA static uint8_t DMA_Data;
_DMA static uint8_t DMA_Command;

/* the data size to OLED , format not change */
#define oled_block_size  (512)
_DMA uint8_t Block[oled_block_size];
_DMA uint8_t Block2[oled_block_size];

/*SPI Event*/
//static volatile uint32_t SPIEvent = 0;
//SemaphoreHandle_t xSemaphore;
static volatile uint32_t g_run_flag = 0;

static inline void INIT_XFER_FLAG() { g_run_flag = 0; }
static inline void SET_XFER_DONE() { g_run_flag |= 1; }
static inline void CLR_XFER_DONE() { g_run_flag &= ~0x1UL; }
static inline bool GET_XFER_DONE() { return (g_run_flag & 1); }

// SPI event
static void SPI_DrvEvent (uint32_t event, uint32_t usr_param) {
//    SPIEvent |= event;

    //notify SPI to send next block of data
    if (event & CSK_SPI_EVENT_TRANSFER_COMPLETE){
        SET_XFER_DONE();
    }

//    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//    if (event & CSK_SPI_EVENT_TRANSFER_COMPLETE){
//        xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
//    }
//    if (xHigherPriorityTaskWoken == PdTRUE) {
//        portYIELD_FROM_ISR();
//    }

}

// wait receive done with timeout
static bool wait_xfer_done_timeout(uint32_t max_wait_ms)
{
    bool ret = false;

    nos_timer_start();
    while(nos_timer_elapsed() < max_wait_ms){
        if(GET_XFER_DONE()){
            CLR_XFER_DONE();
            ret = true;
            break;
        }
    }
    nos_timer_stop();
    return ret;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  OLED interrface define
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/*
//SCLK operate
void OLED_SCLK_Clr(void) {
    GPIO_PinWrite(gGpioDev, (1UL << SPI_CLK_PIN), 0);
}
void OLED_SCLK_Set(void) {
    GPIO_PinWrite(gGpioDev, (1UL << SPI_CLK_PIN), 1);
}
//SDIN operate
void OLED_SDIN_Clr(void) {
    GPIO_PinWrite(gGpioDev, (1UL << SPI_MOSI_PIN), 0);
}
void OLED_SDIN_Set(void) {
    GPIO_PinWrite(gGpioDev, (1UL << SPI_MOSI_PIN), 1);
}
*/

//CS operate
void OLED_BLK_Clr(void) {
	GPIO_PinWrite(gGpioDev, (1UL << BLK_PIN_NUM), 0);
    return;
}
void OLED_BLK_Set(void) {
	GPIO_PinWrite(gGpioDev, (1UL << BLK_PIN_NUM), 1); //BSD: MUST ADD!!
    return;
}
//RESET operate
void OLED_RST_Clr(void) {
    GPIO_PinWrite(gGpioDev, (1UL << RESET_PIN_NUM), 0);
}
void OLED_RST_Set(void) {
    GPIO_PinWrite(gGpioDev, (1UL << RESET_PIN_NUM), 1);
}
//DC operate
void OLED_DC_Clr(void) {
    GPIO_PinWrite(gGpioDev, (1UL << DC_PIN_NUM), 0);
}
void OLED_DC_Set(void) {
    GPIO_PinWrite(gGpioDev, (1UL << DC_PIN_NUM), 1);
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  SPI command operate
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void LCD_WR_REG(unsigned char Data)
{
    uint32_t tick;

    DMA_Command = Data;

//    SPIEvent = 0;
    OLED_DC_Clr();
    SPI_Send(gSpiDev, &DMA_Command, 1);
    if (!wait_xfer_done_timeout(1000)) { // 1000ms
        CLOGD("%s: SPI transfer is blocked!!\n", __func__);
        exit(-1);
    }

    OLED_DC_Set();
}

void LCD_WR_DATA8(unsigned char Data)
{
    uint32_t tick;

    DMA_Data = Data;

//    SPIEvent = 0;
    OLED_DC_Set();
//    uDelay(1);
    SPI_Send(gSpiDev, &DMA_Data, 1);
    if (!wait_xfer_done_timeout(1000)) { // 1000ms
        CLOGD("%s: SPI transfer is blocked!!\n", __func__);
        exit(-1);
    }

    OLED_DC_Set();
}

void Set_Write_RAM()
{
    LCD_WR_REG(0x2C);
}

void Set_Column_Address(unsigned char a, unsigned char b)
{
    LCD_WR_REG(0x2a);
    LCD_WR_DATA8(a>>8);
    LCD_WR_DATA8(a);
    LCD_WR_DATA8(b>>8);
    LCD_WR_DATA8(b);
}

void Set_Row_Address(unsigned char a, unsigned char b)
{
    LCD_WR_REG(0x2b);
    LCD_WR_DATA8(a>>8);
    LCD_WR_DATA8(a);
    LCD_WR_DATA8(b>>8);
    LCD_WR_DATA8(b);
}

void LCD_Clear()
{
    uint16_t i, j;
    uint32_t tick;

    // 0xff is the background color of LCD?
    memset(Block, 0xff, oled_block_size);

    Set_Column_Address(0, Max_Column-1);
    Set_Row_Address(0, Max_Row-1);
    Set_Write_RAM();

    for(i = 0; i <= (2*(Max_Row*Max_Column)/oled_block_size) ; i++){
        SPI_Send(gSpiDev, Block, oled_block_size);
        if (!wait_xfer_done_timeout(5000)) { // 5000ms
            CLOGD("%s: SPI transfer is blocked!!\n", __func__);
            exit(-1);
        }

    } //end for
}


void OLED_Init(void)
{
//    OLED_CS_Set();
    OLED_RST_Clr();
//    vTaskDelay(10);
    nos_delay_ms(100); // 100ms

    OLED_RST_Set();
//    vTaskDelay(2);
    nos_delay_ms(20); // 20ms
    OLED_BLK_Set();

    //************* Start Initial Sequence **********//
    LCD_WR_REG(0x36);
    LCD_WR_DATA8(0x00);

    LCD_WR_REG(0x3A);
    LCD_WR_DATA8(0x05);

    LCD_WR_REG(0xB2);
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x33);
    LCD_WR_DATA8(0x33);

    LCD_WR_REG(0xB7);
    LCD_WR_DATA8(0x35);

    LCD_WR_REG(0xBB);
    LCD_WR_DATA8(0x19);

    LCD_WR_REG(0xC0);
    LCD_WR_DATA8(0x2C);

    LCD_WR_REG(0xC2);
    LCD_WR_DATA8(0x01);

    LCD_WR_REG(0xC3);
    LCD_WR_DATA8(0x12);

    LCD_WR_REG(0xC4);
    LCD_WR_DATA8(0x20);

    LCD_WR_REG(0xC6);
    LCD_WR_DATA8(0x0F);

    LCD_WR_REG(0xD0);
    LCD_WR_DATA8(0xA4);
    LCD_WR_DATA8(0xA1);

    LCD_WR_REG(0xE0);
    LCD_WR_DATA8(0xD0);
    LCD_WR_DATA8(0x04);
    LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x11);
    LCD_WR_DATA8(0x13);
    LCD_WR_DATA8(0x2B);
    LCD_WR_DATA8(0x3F);
    LCD_WR_DATA8(0x54);
    LCD_WR_DATA8(0x4C);
    LCD_WR_DATA8(0x18);
    LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x0B);
    LCD_WR_DATA8(0x1F);
    LCD_WR_DATA8(0x23);

    LCD_WR_REG(0xE1);
    LCD_WR_DATA8(0xD0);
    LCD_WR_DATA8(0x04);
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x11);
    LCD_WR_DATA8(0x13);
    LCD_WR_DATA8(0x2C);
    LCD_WR_DATA8(0x3F);
    LCD_WR_DATA8(0x44);
    LCD_WR_DATA8(0x51);
    LCD_WR_DATA8(0x2F);
    LCD_WR_DATA8(0x1F);
    LCD_WR_DATA8(0x1F);
    LCD_WR_DATA8(0x20);
    LCD_WR_DATA8(0x23);

    LCD_WR_REG(0x21);

    LCD_WR_REG(0x11);
    LCD_WR_REG(0x29);

    LCD_Clear();
}


//void Test_SPI_Send()
//{
//    static uint32_t Data[] = { 0xA5A5A512, 0xFFFFFF34 };
//
//    SPI_Send(gSpiDev, Data, 1);
//
//    if (!wait_xfer_done_timeout(1000)) { // 1000ms
//        CLOGD("%s: SPI transfer is blocked!!\n", __func__);
//        exit(-1);
//    }
//}


void
OLED_Task_init()
{
    //IOMux configuration

//    //!! TEST CODE !!
//    #define SOME_PIN    26
//    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SOME_PIN, CS_PIN_NUMMUX_FUNC_ALTER1); // CSK_IOMUX_FUNC_DEFAULT
//    gGpioDev = GPIOA();
//    GPIO_Initialize(gGpioDev, NULL, NULL);
//    GPIO_Control(gGpioDev, CSK_GPIO_MODE_PULL_NONE | CSK_GPIO_DEBOUNCE_DISABLE, (1UL << SOME_PIN));
//    GPIO_SetDir(gGpioDev, (1UL << SOME_PIN), CSK_GPIO_DIR_OUTPUT);
//
//    while (1) {
//        GPIO_PinWrite(gGpioDev, (1UL << SOME_PIN), 1);
//        nos_delay_ms(10); // ms
//        GPIO_PinWrite(gGpioDev, (1UL << SOME_PIN), 0);
//        nos_delay_ms(10); // ms
//        GPIO_PinWrite(gGpioDev, (1UL << SOME_PIN), 1);
//        nos_delay_ms(10); // ms
//        GPIO_PinWrite(gGpioDev, (1UL << SOME_PIN), 0);
//        nos_delay_ms(10); // ms
//    }

//    outw(CMN_SYSCTRL_BASE + 0x44, 0x04980498);

#if (SPI_INDEX == 0)
    __HAL_CRM_SPI0_CLK_ENABLE();

    // spi0_oled
    IOMuxManager_PinConfigure(SPI0_CS_PIN);
    IOMuxManager_PinConfigure(SPI0_CLK_PIN);
    IOMuxManager_PinConfigure(SPI0_MOSI_PIN);
    //IOMuxManager_PinConfigure(SPI0_MISO_PIN);
    gSpiDev = SPI0();

#elif (SPI_INDEX == 1)
    __HAL_CRM_SPI1_CLK_ENABLE();

    // spi1_oled
    IOMuxManager_PinConfigure(SPI1_CS_PIN);
    IOMuxManager_PinConfigure(SPI1_CLK_PIN);
    IOMuxManager_PinConfigure(SPI1_MOSI_PIN);
    //IOMuxManager_PinConfigure(SPI1_MISO_PIN);
    gSpiDev = SPI1();

#elif (SPI_INDEX == 2)
    __HAL_CRM_SPI2_CLK_ENABLE();

    // spi2_oled
    IOMuxManager_PinConfigure(SPI2_CS_PIN);
    IOMuxManager_PinConfigure(SPI2_CLK_PIN);
    IOMuxManager_PinConfigure(SPI2_MOSI_PIN);
    //IOMuxManager_PinConfigure(SPI2_MISO_PIN);
    gSpiDev = SPI2();

#endif // SPI_INDEX

    // common used pin
    IOMuxManager_PinConfigure(DC_PIN);
    IOMuxManager_PinConfigure(RESET_PIN);
    IOMuxManager_PinConfigure(BLK_PIN);

    gGpioDev = GPIOA();

    //GPIO configuration
    GPIO_Initialize(gGpioDev, NULL, NULL);
    GPIO_SetDir(gGpioDev, (1UL << BLK_PIN_NUM) | (1UL << DC_PIN_NUM) | (1UL << RESET_PIN_NUM), CSK_GPIO_DIR_OUTPUT);

    //SPI configuration
    uint32_t bus_speed = SPI_BUS_SPEED;
    SPI_Initialize(gSpiDev, SPI_DrvEvent, (uint32_t)gSpiDev);
    SPI_PowerControl (gSpiDev, CSK_POWER_FULL);
    SPI_Control(gSpiDev, CSK_SPI_MODE_MASTER | CSK_SPI_TXIO_PIO | //FIXME: CSK_SPI_TXIO_DMA
                 CSK_SPI_CPOL1_CPHA1 | CSK_SPI_RXIO_PIO |
                 CSK_SPI_DATA_BITS(SPI_DATA_BITS) |
                 CSK_SPI_MSB_LSB, bus_speed);

    bus_speed = SPI_Control(gSpiDev, CSK_SPI_GET_BUS_SPEED, 0);
    CLOGD("Current actual SPI speed: %d", bus_speed);

    // divided by 2 if bus speed is greater than 100MHz
    if (bus_speed > 100000000) { // 100MHz
        bus_speed /= 2;
        SPI_Control(gSpiDev, CSK_SPI_SET_BUS_SPEED, bus_speed);
        bus_speed = SPI_Control(gSpiDev, CSK_SPI_GET_BUS_SPEED, 0);
        CLOGD("Current newest SPI speed: %d", bus_speed);
    }

    //TEST ONLY!!
    //Test_SPI_Send();

    //oled configuration
    OLED_Init();
}

extern const unsigned char gImage_chipsky[8192];

void lcd_show_task(void *arg){
    (void)arg;

//    xSemaphore = xSemaphoreCreateBinary();

    OLED_Task_init();

    Set_Column_Address(0, 63);
    Set_Row_Address(0, 63);
    Set_Write_RAM();
    OLED_DC_Set();

    while(1) {

    const uint8_t *d = gImage_chipsky;
    uint32_t size = sizeof(gImage_chipsky); // 8192;
    uint32_t count = 0;

    INIT_XFER_FLAG();

    while (size > 0) {
        count = size >= oled_block_size ? oled_block_size : size;
        //count = size >= 400 ? 400 : size;

        SPI_Send(gSpiDev, d, count);

        d += count;
        size -= count;

//        xSemaphoreTake(xSemaphore, portMAX_DELAY);

//        if (!wait_xfer_done_timeout(5000)) { // 5000ms
//            CLOGD("%s: SPI transfer is blocked!!\n", __func__);
//            break;
//        }

        SPI_Wait_Done(gSpiDev);

/*
        // blocked send in task context
        SPI_Send_Sync(gSpiDev, d, count);
        d += count;
        size -= count;
*/

    } // end while

    nos_delay_ms(1000);

    }

    while(1);
}


/*
void spi_initial_system(uint8_t spi_num)
{
	if (spi_num > 2)
		return;

	//Enable spi_clock
	uint32_t addr = CMN_SYSCTRL_BASE + 0x30 + (spi_num << 2);
	uint32_t rdata = inw(addr);
	rdata &= ~(0x7UL << 28); // clear N to 0
	rdata &= ~(0x7UL << 25); // clear M-1 to 0
	rdata |= (1 << 24) | (0 << 25) | (1 << 28) | (1 << 31);
	outw(addr, rdata);

//	//Reset spi
//	addr = CMN_SYSCTRL_BASE + 0x04;
//	rdata = inw(addr);
//	rdata |= 1 << (spi_num + 3);
//	outw(addr, rdata);
}
*/


int main()
{
//    BootClock_Init();

    logInit(0, 115200); // uart0, baudrate=115200
//    logInit(2, 115200); // uart2, baudrate=115200

//    extern void test_divNM();
//    test_divNM();

    // spi0 / spi1 clock init
    //spi_initial_system(0);
    //spi_initial_system(1);

    // enable global interrupts (all levels of interrupts)
    enable_GINT();

    nos_timer_init();

    lcd_show_task(0);

    while(1);

    return 0;

//    xTaskCreate(task, "lcd_show_task", 8192, NULL, 20, NULL);
//    vTaskStartScheduler();
}
