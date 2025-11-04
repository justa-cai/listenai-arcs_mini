#include "chip.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"

#include "config.h"
#include "sw_uart.h"


/* delay_us(7) : Baud rate is 115200 */
#define SW_UART_DELAY_US        7

#define SW_UART_TX_PIN          2
#define SW_UART_IOMUX_PAD       CSK_IOMUX_PAD_A
#define SW_UART_IOMUX_FUN       CSK_IOMUX_FUNC_DEFAULT


/********************************************************************************/
#define SW_UART_TX_SET()        GPIO_PinWrite(gpiodev, (1UL << SW_UART_TX_PIN), 1)
#define SW_UART_TX_CLR()        GPIO_PinWrite(gpiodev, (1UL << SW_UART_TX_PIN), 0)
#define SW_UART_DELAY()         DELAY_US(SW_UART_DELAY_US)


/********************************************************************************/
static void *gpiodev;

/* delay_us(7) : Baud rate is 115200 */
int sw_uart_init(void)
{
    IOMuxManager_PinConfigure(SW_UART_IOMUX_PAD, SW_UART_TX_PIN, SW_UART_IOMUX_FUN);

    if(SW_UART_IOMUX_PAD == CSK_IOMUX_PAD_A) {
        gpiodev = GPIOA();
    } else {
        gpiodev = GPIOB();
    }

    GPIO_SetDir(gpiodev, (1UL << SW_UART_TX_PIN), CSK_GPIO_DIR_OUTPUT);

    return 0;
}


void sw_uart_send_byte(const uint8_t data)
{
    uint8_t i = 0;
    uint8_t tmp = 0;

    // start bit
    SW_UART_TX_CLR();
    SW_UART_DELAY();

    for(i = 0; i < 8; i++)
    {
        tmp = (data >> i) & 0x01;

        if(tmp == 0)
        {
            SW_UART_TX_CLR();
            SW_UART_DELAY();
        }
        else
        {
            SW_UART_TX_SET();
            SW_UART_DELAY();
        }
    }

    // end bit
    SW_UART_TX_SET();
    SW_UART_DELAY();
}


/**********************************************************************************
**********************************************************************************/
/*
put a char, send to serial port in order to display on the host terminal
parameter:    c: the char
return:        void
*/
static void sw_putch(const char c)
{
    sw_uart_send_byte(c);
}


/**********************************************************************************
**********************************************************************************/
/*
put a string, send to serial port in ordet to display on the host terminal
parameter:    cp: pointer points to the target string
return:        void
*/
static void sw_puts(const char *cp)
{
    int i=0;
    while((i<256) && (cp[i]))
    {
//        if(cp[i]=='\n')
//            putch('\r');
        sw_putch(cp[i]);
        i++;
    }
}


/*********************************************************************************/
/*
    sw_printf("test %d\n", cnt);                    OK
    sw_printf("test 0x%x\n", cnt);                  OK
    sw_printf("test 0x%08x\n", cnt);                OK
    sw_printf("test %#x\n", cnt);                   OK
    sw_printf("test %f\n", 3.14);                   OK
    sw_printf("test %p\n", &cnt);                   OK
    sw_printf("[%s:%d] \n", __func__, __LINE__);    OK
 */
/* delay_us(7) : Baud rate is 115200 */
int sw_printf(const char *fmt, ...)
{
    char buf[128] = {0};
    va_list args;
    int i;

    va_start(args, fmt);

       // i = vsprintf(buf, fmt, args);
    i = vsnprintf(buf, sizeof(buf), fmt, args);   /* hopefully i < sizeof(buf)-4 */
    va_end(args);

    sw_puts(buf);
    return i;
}




