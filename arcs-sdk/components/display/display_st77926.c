#include <string.h>
#include "lisa_display.h"
#include "display_st77926.h"
#include "log_print.h"
#include "systick.h"
#include "PSRAMManager.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "Driver_GPDMA.h"
#include "Driver_QSPI_LCD.h"
#include "FreeRTOS.h"
#include "semphr.h"

typedef struct
{
	uint8_t cmd;
	uint8_t data_len;
	uint8_t data[32];
} init_cmd_item_t;

#define DRV_QSPI_DMA_CH gp_dma_ch1
#define DRV_QSPI_CLK_HZ 50000000

#define QSPI_LCD_IOMUX_PAD CSK_IOMUX_PAD_A
#define QSPI_LCD_IOMUX_FUN CSK_IOMUX_FUNC_ALTER30
#define QSPI_LCD_CS_PIN    19 // LCD_H0146_SPI_CS
#define QSPI_LCD_CLK_PIN   17 // LCD_H0146_SPI_CLK0
#define QSPI_LCD_D0        15 // LCD_H0146_SPI_SIO0
#define QSPI_LCD_D1        14 // LCD_H0146_SPI_SIO1
#define QSPI_LCD_D2        18 // LCD_H0146_SPI_SIO2
#define QSPI_LCD_D3        16 // LCD_H0146_SPI_SIO3
#define QSPI_LCD_RESET_PIN 12  // LCD_REST
#define QSPI_LCD_PWM_PIN   13  // LCD_PWM
#define QSPI_LCD_TE_PIN	   0

#define SW_QSPI_CS_SET() GPIO_PinWrite(gGpioADev, (1UL << QSPI_LCD_CS_PIN), 1)
#define SW_QSPI_CS_CLR() GPIO_PinWrite(gGpioADev, (1UL << QSPI_LCD_CS_PIN), 0)
#define SW_QSPI_RET_SET() GPIO_PinWrite(gGpioBDev, (1UL << QSPI_LCD_RESET_PIN), 1)
#define SW_QSPI_RET_CLR() GPIO_PinWrite(gGpioBDev, (1UL << QSPI_LCD_RESET_PIN), 0)

#define ST77926_CMD_SLEEP_IN 0x10
#define ST77926_CMD_SLEEP_OUT 0x11
#define ST77926_CMD_INV_OFF 0x20
#define ST77926_CMD_INV_ON 0x21
#define ST77926_CMD_GAMSET 0x26
#define ST77926_CMD_DISP_OFF 0x28
#define ST77926_CMD_DISP_ON 0x29
#define ST77926_CMD_CASET 0x2a
#define ST77926_CMD_RASET 0x2b
#define ST77926_CMD_RAMWR 0x2c
#define ST77926_CMD_TEON 0x35

static void *gSpiDev = NULL;
static void *gGpioBDev = NULL;
static void *gGpioADev = NULL;
static SemaphoreHandle_t dma_sem = NULL;

struct st77926_display_send_ctx {
	const uint8_t *buffer;
	uint32_t h;
	uint32_t w;
	uint32_t curr_w;
	uint32_t start_offset;
	uint8_t pixel_size;
};

struct st77926_display_obj {
	enum display_orientation orientation;	
	struct st77926_display_send_ctx display_ctx;
};

static struct st77926_display_obj g_display_obj;

#define delay_ms SysTick_Delay_Ms

static int32_t st77926_set_display_datas_with_dma_rotate_90(uint8_t *data, uint32_t datas_size, uint32_t w, uint32_t h);

#if CONFIG_LISA_DISPLAY_TE_SYNC
static SemaphoreHandle_t te_sem = NULL;
static uint8_t te_pin = 0;
static void GPIO_TE_EventCallback(uint32_t event, void *workspace)
{
    if (event & (1UL << QSPI_LCD_TE_PIN)) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(te_sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
#endif

static inline void LCD_WR_BUS(uint8_t data)
{
	QSPI_LCD_Control(gSpiDev, CSK_QSPI_LCD_RESET_FIFO, 0);
	QSPI_LCD_Send(gSpiDev, &data, 1);
	QSPI_LCD_Wait_Done(gSpiDev);
}

static inline void LCD_WR_REG(uint8_t reg)
{
	LCD_WR_BUS(0x02);
	LCD_WR_BUS(0x00);
	LCD_WR_BUS(reg);
	LCD_WR_BUS(0x00);
}

static inline void LCD_WR_REG_4DATA_MODE(uint8_t reg)
{
	LCD_WR_BUS(0x32);
	LCD_WR_BUS(0x00);
	LCD_WR_BUS(reg);
	LCD_WR_BUS(0x00);
}

static int _write_cmd(uint8_t reg, uint8_t *buf, uint32_t len)
{
	SW_QSPI_CS_CLR();
	LCD_WR_REG(reg);
	for (uint32_t i = 0; i < len; i++)
	{
		LCD_WR_BUS(buf[i]);
	}
	SW_QSPI_CS_SET();
	return 0;
}

static void qspi_lcd_gpdma_callback(uint32_t event, void *workspace)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	struct st77926_display_send_ctx *display_ctx = &g_display_obj.display_ctx;

	// CLOGD("[%s] event=%d", __FUNCTION__, event);
	if(g_display_obj.orientation != DISPLAY_ORIENTATION_NORMAL){
		if (display_ctx->buffer) {
			if (display_ctx->curr_w < display_ctx->w) {
				display_ctx->start_offset -= 2;
				st77926_set_display_datas_with_dma_rotate_90(
					(void *)(display_ctx->buffer +
							display_ctx->start_offset),
					display_ctx->pixel_size * display_ctx->h,
					display_ctx->w, display_ctx->h);

				display_ctx->curr_w++;
			} else {
				display_ctx->buffer = NULL;
				xSemaphoreGiveFromISR(dma_sem, &xHigherPriorityTaskWoken);
				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			}
		}
	}else{
		xSemaphoreGiveFromISR(dma_sem, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

static void QSPI_DrvEvent(uint32_t event, uint32_t usr_param)
{
	// notify SPI to send next block of data
	if (event & CSK_QSPI_LCD_EVENT_TRANSFER_COMPLETE)
	{
	}

	// CLOG("[%s:%d] event=%d", __func__, __LINE__, event);
}

static init_cmd_item_t init_items[] = {
	{.cmd = 0xF1, .data_len = 1, .data = {0x00}},
	{.cmd = 0x60, .data_len = 3, .data = {0x00, 0x00, 0x00}},
	{.cmd = 0x65, .data_len = 1, .data = {0x80}},
	{.cmd = 0x79, .data_len = 1, .data = {0x06}},
	{.cmd = 0x7B, .data_len = 3, .data = {0x00, 0x08, 0x08}},
	{.cmd = 0x80, .data_len = 11, .data = {0x55, 0x62, 0x2F, 0x17, 0xF0, 0x52, 0x70, 0xD2, 0x52, 0x62, 0xEA}},
	{.cmd = 0x81, .data_len = 4, .data = {0x26, 0x52, 0x72, 0x27}},
	{.cmd = 0x84, .data_len = 2, .data = {0x92, 0x25}},
	{.cmd = 0x87, .data_len = 6, .data = {0x10, 0x10, 0x58, 0x00, 0x02, 0x3A}},
	{.cmd = 0x88, .data_len = 15, .data = {0x00, 0x00, 0x2C, 0x10, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x06}},
	{.cmd = 0x89, .data_len = 3, .data = {0x00, 0x00, 0x00}},
	{.cmd = 0x8A, .data_len = 11, .data = {0x13, 0x00, 0x2C, 0x00, 0x00, 0x2C, 0x10, 0x10, 0x00, 0x3E, 0x19}},
	{.cmd = 0x8B, .data_len = 9, .data = {0x15, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x97, 0x8E}},
	{.cmd = 0x8C, .data_len = 13, .data = {0x1D, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x50, 0x0F, 0x01, 0xC5, 0x12, 0x09}},
	{.cmd = 0x8D, .data_len = 1, .data = {0x0C}},
	{.cmd = 0x8E, .data_len = 6, .data = {0x33, 0x01, 0x0C, 0x13, 0x01, 0x01}},
	{.cmd = 0xB3, .data_len = 2, .data = {0x00, 0x30}},
	{.cmd = 0xF1, .data_len = 1, .data = {0x00}},
	{.cmd = 0x71, .data_len = 1, .data = {0xD0}},
	{.cmd = 0x66, .data_len = 2, .data = {0x02, 0x3F}},
	{.cmd = 0xBE, .data_len = 3, .data = {0x20, 0x00, 0x9D}},
	{.cmd = 0x70, .data_len = 12, .data = {0x01, 0xA4, 0x11, 0x40, 0xE0, 0x00, 0x0F, 0x65, 0x00, 0x00, 0x00, 0x1A}},
	{.cmd = 0x90, .data_len = 9, .data = {0x00, 0x44, 0x55, 0x31, 0xC4, 0x71, 0x44, 0x61, 0x61}},
	{.cmd = 0x91, .data_len = 9, .data = {0x00, 0x44, 0x55, 0x36, 0x00, 0x76, 0x40, 0x61, 0x61}},
	{.cmd = 0x92, .data_len = 10, .data = {0x00, 0x44, 0x55, 0x37, 0x00, 0x37, 0x00, 0x05, 0x61, 0x61}},
	{.cmd = 0x93, .data_len = 10, .data = {0x00, 0x43, 0x11, 0x00, 0x00, 0x00, 0x00, 0x05, 0x61, 0x61}},
	{.cmd = 0x94, .data_len = 6, .data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{.cmd = 0x95, .data_len = 5, .data = {0x99, 0x19, 0x00, 0x00, 0xFF}},
	{.cmd = 0x96, .data_len = 12, .data = {0x44, 0x44, 0x07, 0x16, 0x00, 0x20, 0x04, 0x03, 0xC8, 0x61, 0x00, 0x40}},
	{.cmd = 0x97, .data_len = 12, .data = {0x44, 0x44, 0x25, 0x34, 0x00, 0x20, 0x02, 0x01, 0xC8, 0x61, 0x00, 0x40}},
	{.cmd = 0xBA, .data_len = 5, .data = {0x44, 0xC8, 0x61, 0xC8, 0x61}},
	{.cmd = 0x9A, .data_len = 7, .data = {0x40, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}},
	{.cmd = 0x9B, .data_len = 7, .data = {0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}},
	{.cmd = 0x9C, .data_len = 13, .data = {0x40, 0x12, 0x00, 0x00, 0x10, 0x12, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00}},
	{.cmd = 0x9D, .data_len = 8, .data = {0x8C, 0x51, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01}},
	{.cmd = 0x9E, .data_len = 7, .data = {0x51, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01}},
	{.cmd = 0xB4, .data_len = 12, .data = {0x14, 0x1E, 0x10, 0x1C, 0x19, 0x1D, 0x1E, 0x18, 0x03, 0x0A, 0x01, 0x08}},
	{.cmd = 0xB5, .data_len = 12, .data = {0x12, 0x1E, 0x10, 0x1C, 0x19, 0x1D, 0x1E, 0x18, 0x02, 0x0B, 0x00, 0x09}},
	{.cmd = 0xB6, .data_len = 7, .data = {0x99, 0x99, 0x00, 0x0F, 0xBF, 0x0F, 0xBF}},
	{.cmd = 0x86, .data_len = 14, .data = {0xC9, 0x04, 0xB1, 0x02, 0x58, 0x12, 0x58, 0x0C, 0x13, 0x01, 0xA5, 0x00, 0xA5, 0xA5}},
	{.cmd = 0xB7, .data_len = 16, .data = {0x00, 0x0C, 0x0C, 0x0E, 0x0B, 0x07, 0x34, 0x03, 0x04, 0x49, 0x08, 0x15, 0x15, 0x2C, 0x32, 0x0F}},
	{.cmd = 0xB8, .data_len = 16, .data = {0x00, 0x0C, 0x0C, 0x0E, 0x0B, 0x06, 0x34, 0x03, 0x04, 0x49, 0x08, 0x14, 0x14, 0x2C, 0x32, 0x0F}},
	{.cmd = 0xB9, .data_len = 2, .data = {0x23, 0x23}},
	{.cmd = 0xBF, .data_len = 6, .data = {0x0D, 0x11, 0x11, 0x0B, 0x0B, 0x0B}},
	{.cmd = 0xF2, .data_len = 1, .data = {0x00}},
	{.cmd = 0x73, .data_len = 5, .data = {0x04, 0xDA, 0x12, 0x52, 0x51}},
	{.cmd = 0x77, .data_len = 5, .data = {0x6B, 0x5B, 0xFD, 0xC3, 0xC5}},
	{.cmd = 0x7A, .data_len = 2, .data = {0x15, 0x27}},
	{.cmd = 0x7B, .data_len = 2, .data = {0x04, 0x57}},
	{.cmd = 0x7E, .data_len = 2, .data = {0x01, 0x0E}},
	{.cmd = 0xBF, .data_len = 1, .data = {0x36}},
	{.cmd = 0xE3, .data_len = 2, .data = {0x40, 0x40}},
	{.cmd = 0xF0, .data_len = 1, .data = {0x00}},
	{.cmd = 0xD0, .data_len = 1, .data = {0x00}},
	{.cmd = 0x2A, .data_len = 4, .data = {0x00, 0x00, 0x01, 0x3F}},
	{.cmd = 0x2B, .data_len = 4, .data = {0x00, 0x00, 0x01, 0xDF}},
	{.cmd = 0x35, .data_len = 1, .data = {0x00}},
	{.cmd = 0x3A, .data_len = 1, .data = {0x01}},
	{.cmd = 0xF3, .data_len = 1, .data = {0x00}},
	{.cmd = 0x70, .data_len = 2, .data = {0x00}},
};

static void lcd_st77926_init(void)
{
	// reset
	SW_QSPI_RET_SET();
	delay_ms(150);
	SW_QSPI_RET_CLR();
	delay_ms(150);
	SW_QSPI_RET_SET();
	delay_ms(200);

	for (uint32_t i = 0; i < sizeof(init_items) / sizeof(init_items[0]); i++)
	{
		_write_cmd(init_items[i].cmd, init_items[i].data, init_items[i].data_len);
	}

	_write_cmd(ST77926_CMD_INV_ON, NULL, 0);

	_write_cmd(ST77926_CMD_SLEEP_OUT, NULL, 0);

	delay_ms(120);
}

static void lcd_st77926_set_mem_area(const uint16_t x, const uint16_t y, const uint16_t w, const uint16_t h)
{
	uint8_t buf[4];

	buf[0] = x >> 8;
	buf[1] = x & 0xff;
	buf[2] = (x + w - 1) >> 8;
	buf[3] = (x + w - 1) & 0xff;
	_write_cmd(ST77926_CMD_CASET, buf, 4);

	buf[0] = y >> 8;
	buf[1] = y & 0xff;
	buf[2] = (y + h - 1) >> 8;
	buf[3] = (y + h - 1) & 0xff;
	_write_cmd(ST77926_CMD_RASET, buf, 4);
}

int st77926_display_init(display_hw_config_t *config)
{
	int ret = 0;
	uint32_t control = 0;

	// PINMUX
	IOMuxManager_PinConfigure(QSPI_LCD_IOMUX_PAD, QSPI_LCD_D3, QSPI_LCD_IOMUX_FUN);      // LCD_H0146_SPI_SIO3
	IOMuxManager_PinConfigure(QSPI_LCD_IOMUX_PAD, QSPI_LCD_CLK_PIN, QSPI_LCD_IOMUX_FUN); // LCD_H0146_SPI_CLK
	IOMuxManager_PinConfigure(QSPI_LCD_IOMUX_PAD, QSPI_LCD_D2, QSPI_LCD_IOMUX_FUN); // LCD_H0146_SPI_SIO2
	IOMuxManager_PinConfigure(QSPI_LCD_IOMUX_PAD, QSPI_LCD_D1, QSPI_LCD_IOMUX_FUN); // LCD_H0146_SPI_SIO1
	IOMuxManager_PinConfigure(QSPI_LCD_IOMUX_PAD, QSPI_LCD_D0, QSPI_LCD_IOMUX_FUN); // LCD_H0146_SPI_SIO0
	IOMuxManager_PinConfigure(QSPI_LCD_IOMUX_PAD, QSPI_LCD_PWM_PIN, CSK_IOMUX_FUNC_ALTER12); // LCD_PWM

	// CONTROL GPIO
	gGpioADev = GPIOA();
	GPIO_Initialize(gGpioADev, NULL, NULL);

	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, QSPI_LCD_CS_PIN, CSK_IOMUX_FUNC_DEFAULT); // GPIO QSPI_CS
	GPIO_SetDir(gGpioADev, (1UL << QSPI_LCD_CS_PIN), CSK_GPIO_DIR_OUTPUT);
	GPIO_PinWrite(gGpioADev, (1UL << QSPI_LCD_CS_PIN), 1);

	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, QSPI_LCD_RESET_PIN, CSK_IOMUX_FUNC_DEFAULT); // GPIO LCD_RESET
	GPIO_SetDir(gGpioADev, (1UL << QSPI_LCD_RESET_PIN), CSK_GPIO_DIR_OUTPUT);
	GPIO_PinWrite(gGpioADev, (1UL << QSPI_LCD_RESET_PIN), 1);

#if CONFIG_LISA_DISPLAY_TE_SYNC
	gGpioBDev = GPIOB();
	GPIO_Initialize(gGpioBDev, NULL, NULL);
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, QSPI_LCD_TE_PIN, CSK_IOMUX_FUNC_DEFAULT); // GPIO LCD_TE
	GPIO_SetDir(gGpioBDev, (1UL << QSPI_LCD_TE_PIN), CSK_GPIO_DIR_INPUT);
	GPIO_Control(gGpioBDev, CSK_GPIO_DEBOUNCE_DISABLE | CSK_GPIO_SET_INTR_NEGATIVE_EDGE | CSK_GPIO_INTR_ENABLE,
		     (1UL << QSPI_LCD_TE_PIN));
	GPIO_SetCallback(gGpioBDev, (1UL << QSPI_LCD_TE_PIN), GPIO_TE_EventCallback, NULL);
	te_sem = xSemaphoreCreateBinary();
	if (te_sem == NULL) {
		CLOGE("[%s] Failed to create TE semaphore", __FUNCTION__);
		return -1;
	}
#endif

	// DMA INIT
	csk_gpdma_init_t gpdma_cfg = {
		.dma_ch = DRV_QSPI_DMA_CH,
		.burst_len = gpdma_burst_len_8spl,
		.src_mode = address_mode_normal,
		.dst_mode = address_mode_normal,
		.tfr_mode = tfr_mode_m2p,
		.src_inc_mode = inc_mode_increase,
		.dst_inc_mode = inc_mode_fix,
		.prio_lvl = prio_mode_vhigh,
		.sample_unit = gpdma_sample_unit_halfword,
		.handshake = qspi_hs_num0,
	};

	ret = GPDMA_Initialize();
	if (ret != CSK_DRIVER_OK)
	{
		CLOGE("[%s] Failed to initialize GPDMA", __FUNCTION__);
		return -1;
	}

	ret = GPDMA_Config(&gpdma_cfg, qspi_lcd_gpdma_callback, NULL);
	if (ret != CSK_DRIVER_OK)
	{
		CLOGE("[%s] Failed to config GPDMA", __FUNCTION__);
		return -1;
	}

	// QSPI INIT
	gSpiDev = QSPI_LCD();
	if (gSpiDev == NULL)
	{
		CLOGE("[%s] Failed to get QSPI device", __FUNCTION__);
		return -1;
	}

	ret = QSPI_LCD_Initialize(gSpiDev, QSPI_DrvEvent, (uint32_t)gSpiDev);
	if (ret != CSK_DRIVER_OK)
	{
		CLOGE("[%s] Failed to initialize QSPI device", __FUNCTION__);
		return -1;
	}

	ret = QSPI_LCD_PowerControl(gSpiDev, CSK_POWER_FULL);
	if (ret != CSK_DRIVER_OK)
	{
		CLOGE("[%s] Failed to power up QSPI device", __FUNCTION__);
		return -1;
	}

	/*
		LCD初始化过程使用中断模式，后续图像传输时再切DMA模式
	*/
	control |= CSK_QSPI_LCD_TXIO_PIO | CSK_QSPI_LCD_CPOL0_CPHA0 | CSK_QSPI_LCD_MSB_LSB | CSK_QSPI_LCD_MODE_MASTER |
			   CSK_QSPI_LCD_DATA_BITS(8);
	ret = QSPI_LCD_Control(gSpiDev, control, DRV_QSPI_CLK_HZ);
	if (ret != CSK_DRIVER_OK)
	{
		CLOGE("[%s] Failed to control QSPI device", __FUNCTION__);
		return -1;
	}

	ret = QSPI_LCD_SetDatLength(gSpiDev, 8);
	if (ret != CSK_DRIVER_OK)
	{
		CLOGE("[%s] Failed to set data length", __FUNCTION__);
		return -1;
	}

	ret = QSPI_LCD_SetLcdMode(gSpiDev, CSK_QSPI_LCD_MODE_NORMAL);
	if (ret != CSK_DRIVER_OK)
	{
		CLOGE("[%s] Failed to set LCD mode", __FUNCTION__);
		return -1;
	}

	ret = QSPI_LCD_SetLaneNum(gSpiDev, CSK_QSPI_LCD_LANE_NUM_SINGLE);
	if (ret != CSK_DRIVER_OK)
	{
		CLOGE("[%s] Failed to set lane number", __FUNCTION__);
		return -1;
	}

	lcd_st77926_init();

	dma_sem = xSemaphoreCreateBinary();
	if (dma_sem == NULL) {
		CLOGE("[%s] Failed to create DMA semaphore", __FUNCTION__);
		return -1;
	}

	return 0;
}

int st77926_display_blanking_off(void)
{
	_write_cmd(ST77926_CMD_DISP_ON, NULL, 0);

	return 0;
}

int st77926_display_blanking_on(void)
{
	_write_cmd(ST77926_CMD_DISP_OFF, NULL, 0);

	return 0;
}

void st77926_display_get_capabilities(struct display_capabilities *capabilities)
{
	memset(capabilities, 0, sizeof(*capabilities));

	capabilities->x_resolution = CONFIG_DISPLAY_ST77926_WIDTH;
	capabilities->y_resolution = CONFIG_DISPLAY_ST77926_HEIGHT;
	capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;
	capabilities->current_orientation = g_display_obj.orientation;
	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
}

int st77926_display_set_brightness(const uint8_t brightness)
{
	return -1;
}

static int32_t st77926_set_display_datas_with_dma_rotate_90(uint8_t *data,
					     uint32_t datas_size, uint32_t w, uint32_t h)
{
	// CLOGD("[%s] data=%p datas_size = %d, w = %d, h = %d\r\n", __func__, data, datas_size, w, h);
	QSPI_LCD_DMADisable(gSpiDev);

	QSPI_LCD_SetDatLength(gSpiDev, 16);
	QSPI_LCD_SetLaneNum(gSpiDev, CSK_QSPI_LCD_LANE_NUM_QUAD);
	QSPI_LCD_SetDMASize(gSpiDev, datas_size);
	QSPI_LCD_DMAEnable(gSpiDev);

	csk_gpdma_init_t gpdma_cfg;
	memset(&gpdma_cfg, 0, sizeof(gpdma_cfg));

	gpdma_cfg.dma_ch = DRV_QSPI_DMA_CH;
	gpdma_cfg.burst_len = gpdma_burst_len_1spl;
	gpdma_cfg.src_mode = address_mode_normal;
	gpdma_cfg.dst_mode = address_mode_normal;
	gpdma_cfg.tfr_mode = tfr_mode_m2p;
	gpdma_cfg.src_inc_mode = inc_mode_increase;
	gpdma_cfg.dst_inc_mode = inc_mode_fix;
	gpdma_cfg.prio_lvl = prio_mode_vhigh;
	gpdma_cfg.sample_unit = gpdma_sample_unit_halfword;
	gpdma_cfg.handshake = qspi_hs_num0;
	gpdma_cfg.gather_en = 1;
	gpdma_cfg.scatter_en = 0;
	gpdma_cfg.gather_counter = 1;
	gpdma_cfg.gather_interval = w - 1;

	GPDMA_Config(&gpdma_cfg, qspi_lcd_gpdma_callback, NULL);

	GPDMA_Start_Normal(DRV_QSPI_DMA_CH, data, (void *)QSPI_LCD_Buf(), datas_size / 2);

	return 0;
}

int st77926_display_write(const uint16_t x, const uint16_t y, const struct display_buffer_descriptor *desc,
						  const void *buf)
{
	int ret = 0;
	uint16_t new_x, new_y, new_w, new_h;
	uint32_t image_size = desc->buf_size;
	void *image_buf = (void *)buf;

#if CONFIG_LISA_DISPLAY_TE_SYNC
	xQueueReset(te_sem);
	if (xSemaphoreTake(te_sem, pdMS_TO_TICKS(100)) != pdTRUE) {
		CLOGE("[%s] Failed to take TE semaphore", __FUNCTION__);
		return -1;
	}
#endif
	// CLOGD("[%s:%d] buf=%p x=%d, y=%d, w=%d, h=%d, image_size=%d", __func__, __LINE__, buf, x, y, desc->width, desc->height, image_size);

	if(g_display_obj.orientation == DISPLAY_ORIENTATION_ROTATED_90){
		new_x = CONFIG_DISPLAY_ST77926_WIDTH - (y + desc->height);
		new_y = x;
		new_w = desc->height;
		new_h = desc->width;
	}else if(g_display_obj.orientation == DISPLAY_ORIENTATION_ROTATED_270){
		new_x = y;
		new_y = CONFIG_DISPLAY_ST77926_HEIGHT - x - desc->width;  
		new_w = desc->height;
		new_h = desc->width;
	}else{
		new_x = x;
		new_y = y;
		new_w = desc->width;
		new_h = desc->height;
	}

	// Add the configured offsets to the final coordinates
	new_x += CONFIG_DISPLAY_ST77926_X_OFFSET;
	new_y += CONFIG_DISPLAY_ST77926_Y_OFFSET;

	if (image_size % 4 != 0 || ((uint32_t)buf) % 4 != 0) {
		CLOG("[%s:%d] must be 4 byte aligned", __func__, __LINE__);
		return -1;
	}

	// CLOGD("[%s:%d] x=%d, y=%d, w=%d, h=%d, image_size=%d", __func__, __LINE__, new_x, new_y, desc->width, desc->height, image_size);

	lcd_st77926_set_mem_area(new_x, new_y, new_w, new_h);

	SW_QSPI_CS_CLR();

	LCD_WR_REG_4DATA_MODE(ST77926_CMD_RAMWR);
	if(g_display_obj.orientation != DISPLAY_ORIENTATION_NORMAL){
		uint16_t write_h = desc->height;
		struct st77926_display_send_ctx *display_ctx = &g_display_obj.display_ctx;
		display_ctx->buffer = (uint8_t *)buf;
		display_ctx->h = desc->height;
		display_ctx->w = desc->width;
		display_ctx->curr_w = 1;
		display_ctx->pixel_size = 2;
		display_ctx->start_offset = (desc->width - 1) * display_ctx->pixel_size;

		/* 剩下的数据将会在中断中发送 */
		st77926_set_display_datas_with_dma_rotate_90((void *)(display_ctx->buffer + display_ctx->start_offset), display_ctx->pixel_size * write_h, desc->width, desc->height);		
	}else{
		QSPI_LCD_SetDatLength(gSpiDev, 16);
		QSPI_LCD_SetLaneNum(gSpiDev, CSK_QSPI_LCD_LANE_NUM_QUAD);
		QSPI_LCD_SetDMASize(gSpiDev, image_size);
		QSPI_LCD_DMAEnable(gSpiDev);
		GPDMA_Start_Normal(DRV_QSPI_DMA_CH, image_buf, (void *)QSPI_LCD_Buf(), image_size / 2);
	}

	if (xSemaphoreTake(dma_sem, portMAX_DELAY) != pdTRUE) {
		CLOGE("[%s] Failed to take DMA semaphore", __FUNCTION__);
		return -1;
	}

	QSPI_LCD_DMADisable(gSpiDev);
	SW_QSPI_CS_SET();
	QSPI_LCD_SetLaneNum(gSpiDev, CSK_QSPI_LCD_LANE_NUM_SINGLE);
	QSPI_LCD_SetDatLength(gSpiDev, 8);

	return ret;
}

int st77926_display_set_orientation(const enum display_orientation orientation)
{
	if(orientation != DISPLAY_ORIENTATION_NORMAL && orientation != DISPLAY_ORIENTATION_ROTATED_270){
		return -1;
	}

	g_display_obj.orientation = orientation;
	return 0;
}

int st77926_display_sleep(const uint8_t onoff)
{
	if(onoff){
		_write_cmd(ST77926_CMD_INV_ON, NULL, 0);
	}else{
		_write_cmd(ST77926_CMD_INV_OFF, NULL, 0);
	}
	return 0;
}

static const struct display_driver_api st77926_driver_api = {
	.display_blanking_on = st77926_display_blanking_on,
	.display_blanking_off = st77926_display_blanking_off,
	.display_get_capabilities = st77926_display_get_capabilities,
	.display_set_brightness = st77926_display_set_brightness,
	.display_write = st77926_display_write,
	.display_set_orientation = st77926_display_set_orientation,
	.display_sleep = st77926_display_sleep,
};

const struct display_device display_st77926 = {
	.name = "st77926",
	.device_init = st77926_display_init,
	.api = &st77926_driver_api,
};