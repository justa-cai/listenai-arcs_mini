#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Driver_DVP.h"
#include "Driver_GPDMA.h"
#include "Driver_SPI.h"
#include "Driver_I2C.h"
#include "Driver_UART.h"
#include "IOMuxManager.h"
#include "ClockManager.h"
#include "lisa_log.h"
#include "camera_xfer.h"
#include "camera_core.h"
#include "board.h"

volatile uint32_t uart_event = 0;
uint8_t uart_buf[614400]  __attribute__((section(".psram.data"))) = {0};
static void UART_EventCallback(uint32_t event, void *workspace)
{
	uart_event |= event;
}

static void *UART_Handler = NULL;

#define UART1_IO_TX_PAD (CSK_IOMUX_PAD_A)
#define UART1_IO_TX_PIN (21)
#define UART1_IO_TX_SEL (CSK_IOMUX_FUNC_ALTER3)

void serail_init(void)
{
	__HAL_CRM_UART1_CLK_ENABLE();

	IOMuxManager_PinConfigure(UART1_IO_TX_PAD, UART1_IO_TX_PIN, UART1_IO_TX_SEL);
	// IOMuxManager_PinConfigure(UART1_IO_RX_PAD, UART1_IO_RX_PIN, UART1_IO_RX_SEL);

	UART_Handler = UART1();
	UART_Initialize(UART_Handler, UART_EventCallback, NULL);

	UART_PowerControl(UART_Handler, CSK_POWER_FULL);

	UART_Control(UART_Handler,
                CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT | CSK_UART_DATA_BITS_8 |
			     CSK_UART_PARITY_NONE | CSK_UART_STOP_BITS_1 |
			     CSK_UART_FLOW_CONTROL_NONE | CSK_UART_Function_CONTROL_Dma |
			     CSK_UART_GPIO_CONTROL_DEFAULT,
		    3000000);

	UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
	// UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

	uart_event = 0;
}

void serial_send(const uint8_t *s, uint32_t len)
{
	uart_event = 0;

	LOGI("%x %x %x %x %x %x\r\n", s[0], s[1], s[2], s[len - 3], s[len - 2], s[len - 1]);
	int err = UART_Send(UART_Handler, s, len);
	if (err) {
		printf("uart send failed, err: %d\n", err);
		return;
	}
}


int main(int argc, char **argv)
{
    camera_config_t camera_cfg = {
        .xclk_freq_hz = 24000000,
        .pixel_format = PIXFORMAT_YUV422,
        .frame_size   = FRAMESIZE_VGA,
        .buf_count    = 3,
        .colorbar     = 0,
        .i2c_config = {
            .i2c_dev = LISA_CAMERA_I2C_DEV,
            .pins = {
                .sda = {
                    .pad = LISA_CAMERA_I2C_SDA_PORT,
                    .pin = LISA_CAMERA_I2C_SDA_PIN,
                    .func = LISA_CAMERA_I2C_SDA_FUNC
                },
                .scl = {
                    .pad = LISA_CAMERA_I2C_SCL_PORT,
                    .pin = LISA_CAMERA_I2C_SCL_PIN,
                    .func = LISA_CAMERA_I2C_SCL_FUNC
                }
            }
        },
        .xfer_config = {
            .dvp_config = {
                .dvp_dev = DVP0(),
                .input_format = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
                .pck_polarity = DVP_POL_FALLING,
                .vs_polarity  = DVP_POL_FALLING,
                .hs_polarity  = DVP_POL_RISING,
                .data_align   = DVP_DATA_ALIGN_LEFT,
                .dma_channel  = gp_dma_ch3,
                .pins = {
                    .hsync = {
                        .pad = LISA_CAMERA_DVP_HSYNC_PORT,
                        .pin = LISA_CAMERA_DVP_HSYNC_PIN,
                        .func = LISA_CAMERA_DVP_HSYNC_FUNC
                    },
                    .vsync = {
                        .pad = LISA_CAMERA_DVP_VSYNC_PORT,
                        .pin = LISA_CAMERA_DVP_VSYNC_PIN,
                        .func = LISA_CAMERA_DVP_VSYNC_FUNC
                    },
                    .pclk = {
                        .pad = LISA_CAMERA_DVP_PCLK_PORT,
                        .pin = LISA_CAMERA_DVP_PCLK_PIN,
                        .func = LISA_CAMERA_DVP_PCLK_FUNC
                    },
                    .data = {
                        {
                            .pad = LISA_CAMERA_DVP_DATA0_PORT,
                            .pin = LISA_CAMERA_DVP_DATA0_PIN,
                            .func = LISA_CAMERA_DVP_DATA0_FUNC
                        },
                        {
                            .pad = LISA_CAMERA_DVP_DATA1_PORT,
                            .pin = LISA_CAMERA_DVP_DATA1_PIN,
                            .func = LISA_CAMERA_DVP_DATA1_FUNC
                        },
                        {
                            .pad = LISA_CAMERA_DVP_DATA2_PORT,
                            .pin = LISA_CAMERA_DVP_DATA2_PIN,
                            .func = LISA_CAMERA_DVP_DATA2_FUNC
                        },
                        {
                            .pad = LISA_CAMERA_DVP_DATA3_PORT,
                            .pin = LISA_CAMERA_DVP_DATA3_PIN,
                            .func = LISA_CAMERA_DVP_DATA3_FUNC
                        },
                        {
                            .pad = LISA_CAMERA_DVP_DATA4_PORT,
                            .pin = LISA_CAMERA_DVP_DATA4_PIN,
                            .func = LISA_CAMERA_DVP_DATA4_FUNC
                        },
                        {
                            .pad = LISA_CAMERA_DVP_DATA5_PORT,
                            .pin = LISA_CAMERA_DVP_DATA5_PIN,
                            .func = LISA_CAMERA_DVP_DATA5_FUNC
                        },
                        {
                            .pad = LISA_CAMERA_DVP_DATA6_PORT,
                            .pin = LISA_CAMERA_DVP_DATA6_PIN,
                            .func = LISA_CAMERA_DVP_DATA6_FUNC
                        },
                        {
                            .pad = LISA_CAMERA_DVP_DATA7_PORT,
                            .pin = LISA_CAMERA_DVP_DATA7_PIN,
                            .func = LISA_CAMERA_DVP_DATA7_FUNC
                        }
                    }
                }
            },
        }
    };

    serail_init();

    camera_init(&camera_cfg);

    camera_start();

    uart_event = CSK_UART_EVENT_SEND_COMPLETE;
    int send_cnt = 0;
    while (1) {
        struct cam_ipeg_mem *spi_mem = NULL;

        spi_mem = camera_dqbuf(spi_mem, 1000);
        if (spi_mem) {
            if ((uart_event & CSK_UART_EVENT_SEND_COMPLETE) && (send_cnt++ < 20)) {
                memcpy(uart_buf,spi_mem->buf.addr, sizeof(uart_buf));
                uart_event = 0;
                serial_send(uart_buf, spi_mem->buf.size);
            }
            camera_qbuf(spi_mem);
        }
        else {
            LOGE("camera_dqbuf timeout");
        }
    }

    camera_stop();
}
