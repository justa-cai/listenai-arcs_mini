/*
 * GPIO Loop Test
 */

#include "cli_main.h"

#include "chip.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"


/**
 * @brief Configure and test a loopback between two GPIO pins
 * 
 * @param in_port Input GPIO Port Handler
 * @param in_pin_idx Input Pin Index (0-31)
 * @param out_port Output GPIO Port Handler
 * @param out_pin_idx Output Pin Index (0-31)
 */
static void test_gpio_loop_execution(void *in_port, uint32_t in_pin_idx, void *out_port, uint32_t out_pin_idx) {
    int32_t ret;
    uint32_t in_mask = (1UL << in_pin_idx);
    uint32_t out_mask = (1UL << out_pin_idx);
    uint32_t val;

    CLI_LOG("Testing GPIO Loop: IN Port %s Pin %u <-> OUT Port %s Pin %u\r\n", 
        (in_port == GPIOA()) ? "A" : "B", in_pin_idx, (out_port == GPIOA()) ? "A" : "B", out_pin_idx);

    // Configure IOMUX for the pins
    if (in_port == GPIOA()) {
        if (in_pin_idx == 0 || in_pin_idx == 1 || in_pin_idx == 8 || in_pin_idx == 9)
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, in_pin_idx, CSK_IOMUX_FUNC_ALTER1);
        else
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, in_pin_idx, CSK_IOMUX_FUNC_DEFAULT);
    } else {
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, in_pin_idx, CSK_IOMUX_FUNC_DEFAULT);
    }

    if (out_port == GPIOA()) {
        if (out_pin_idx == 0 || out_pin_idx == 1 || out_pin_idx == 8 || out_pin_idx == 9)
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, out_pin_idx, CSK_IOMUX_FUNC_ALTER1);
        else
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, out_pin_idx, CSK_IOMUX_FUNC_DEFAULT);
    } else {
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, out_pin_idx, CSK_IOMUX_FUNC_DEFAULT);
    }
    // 1. Configure Input
    ret = GPIO_SetDir(in_port, in_mask, CSK_GPIO_DIR_INPUT);
    if (ret) {
        CLI_LOG("configure Input fail\n");
        return;
    }

    // 2. Configure Output
    ret = GPIO_SetDir(out_port, out_mask, CSK_GPIO_DIR_OUTPUT);
    if (ret) {
        CLI_LOG("configure Output fail\n");
        return;
    }

    // 3. Test Low
    ret = GPIO_PinWrite(out_port, out_mask, 0);
    //TEST_ASSERT_EQUAL_INT32(CSK_DRIVER_OK, ret);

    val = GPIO_PinRead(in_port, in_mask);
    //TEST_ASSERT_EQUAL_INT32(0, val); // Expect 0
    if (val != 0) {
        CLI_LOGE(" 1st FAIL val %d in_port %x in mask %x, out port %x out mask %x ret %x \n", val, in_port, in_mask, out_port, out_mask, ret);
        goto fail;
    }
    // 4. Test High
    ret = GPIO_PinWrite(out_port, out_mask, 1);
    //TEST_ASSERT_EQUAL_INT32(CSK_DRIVER_OK, ret);

    val = GPIO_PinRead(in_port, in_mask);
    //TEST_ASSERT_EQUAL_INT32(1, val); // Expect 1
    if (val !=1){
        CLI_LOGE("2nd FAIL val %d in_port %x in mask %x, out port %x out mask %x ret %x \n", val, in_port, in_mask, out_port, out_mask, ret);
        goto fail;
    }

    // 5. Test Low
    ret = GPIO_PinWrite(out_port, out_mask, 0);
    //TEST_ASSERT_EQUAL_INT32(CSK_DRIVER_OK, ret);

    val = GPIO_PinRead(in_port, in_mask);
    //TEST_ASSERT_EQUAL_INT32(0, val); // Expect 0
    if (val !=0) {
        CLI_LOGE("3rd FAIL val %d in_port %x in mask %x, out port %x out mask %x ret %x \n", val, in_port, in_mask, out_port, out_mask, ret);
        goto fail;
    }

    CLI_LOG("PASS: High/Low toggle matches.\r\n");
    return;
fail:
    CLI_LOG("FAIL: High/Low toggle not match.\r\n");
    return;
}


int cli_gpio_loop_test(char *params)
{
    char *next = params, *token = NULL, *token_val = NULL;
    uint8_t input_port, input_pin, output_port, output_pin;
    static int init = 0;
    void *inport = NULL, *outport = NULL;
    int res = CLI_SUCCESS;

    while ((token = utils_next_token(&next)) && (token_val = utils_next_token(&next))) {
        if (!token || !token_val) {
            res = CLI_SHOW_USAGE;
            break;
        }

        if (!strcmp("in_port", token))
            input_port = atoi(token_val);

        if (!strcmp("in_pin", token))
            input_pin = atoi(token_val);

        if (!strcmp("out_port", token))
            output_port = atoi(token_val);

        if (!strcmp("out_pin", token))
            output_pin = atoi(token_val);
    }

    CLI_LOG("in port %d in pin %d out port %d output pin %d \n", input_port, input_pin, output_port, output_pin);
    if (input_port != CSK_IOMUX_PAD_A && input_port != CSK_IOMUX_PAD_B || \
        output_port != CSK_IOMUX_PAD_A && output_port != CSK_IOMUX_PAD_B || \
        input_pin > CSK_IOMUX_PAD_A_MAX_PIN || \
        output_pin > CSK_IOMUX_PAD_A_MAX_PIN ) {
        res = CLI_SHOW_USAGE;
    }

   /*if (((input_port == CSK_IOMUX_PAD_A) && (input_pin == 0 || input_pin == 1 || input_pin == 8 || input_pin == 9)) || \
        ((output_port == CSK_IOMUX_PAD_A) && (output_pin == 0 || output_pin == 1 || output_pin == 8 || output_pin == 9))) {
        CLI_LOG("gpioA, gpio pin should not use gpio 0/1/8/9 \n");
        res = CLI_SHOW_USAGE;
    }*/

    if(res)
        return res;

    CLI_LOG("\r\n++++++++++ GPIO In/Out Test ++++++++++\r\n");

    // Initialize GPIO Controllers
    if (!init) {
        GPIO_Initialize(GPIOA(), NULL, NULL);
        GPIO_Initialize(GPIOB(), NULL, NULL);
        init = 1;
    }

    if (input_port == CSK_IOMUX_PAD_A)
        inport = GPIOA();
    else
        inport = GPIOB();

    if (output_port == CSK_IOMUX_PAD_A)
        outport = GPIOA();
    else
        outport = GPIOB();

    test_gpio_loop_execution(inport, input_pin, outport, output_pin);

    return CLI_SUCCESS;
}

void *inport_test = NULL;
uint32_t in_mask_test = 0;
void *outport_test = NULL;
uint32_t out_mask_test = 0;

int cli_gpio_in(char *params)
{
    char *next = params, *token = NULL, *token_val = NULL;
    uint8_t input_port, input_pin, param_cnt;
    uint32_t in_mask ;
    static int init = 0;
    void *inport = NULL;
    int res = CLI_SUCCESS;

    while ((token = utils_next_token(&next)) && (token_val = utils_next_token(&next))) {
        if (!token || !token_val) {
            res = CLI_SHOW_USAGE;
            break;
        }

        if (!strcmp("port", token)) {
            param_cnt ++;
            input_port = atoi(token_val);
        }

        if (!strcmp("pin", token)) {
            param_cnt ++;
            input_pin = atoi(token_val);
        }
    }

    CLI_LOG("in port %d in pin %d  \n", input_port, input_pin);
    if (input_port != CSK_IOMUX_PAD_A && input_port != CSK_IOMUX_PAD_B || \
       (input_port == CSK_IOMUX_PAD_A && input_pin > CSK_IOMUX_PAD_A_MAX_PIN) || \
       (input_port == CSK_IOMUX_PAD_B && input_pin > CSK_IOMUX_PAD_B_MAX_PIN) || \
       param_cnt != 2) {
        res = CLI_SHOW_USAGE;
    }

    /*if (((input_port == CSK_IOMUX_PAD_A) && (input_pin == 0 || input_pin == 1 || input_pin == 8 || input_pin == 9))) {
        CLI_LOG("gpioA, gpio pin should not use gpio 0/1/8/9 \n");
        res = CLI_SHOW_USAGE;
    }*/

    if(res)
        return res;

    CLI_LOG("\r\n++++++++++ GPIO In Test ++++++++++\r\n");

    // Initialize GPIO Controllers
    if (!init) {
        GPIO_Initialize(GPIOA(), NULL, NULL);
        GPIO_Initialize(GPIOB(), NULL, NULL);
        init = 1;
    }


    // Configure IOMUX for the pins
    if (input_port == CSK_IOMUX_PAD_A) {
        inport = GPIOA();
        if (input_pin == 0 || input_pin ==1 || input_pin == 8 || input_pin == 9)
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, input_pin, CSK_IOMUX_FUNC_ALTER1);
        else
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, input_pin, CSK_IOMUX_FUNC_DEFAULT);
    }
    else {
        inport = GPIOB();
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, input_pin, CSK_IOMUX_FUNC_DEFAULT);
    }

    in_mask = (1UL << input_pin);
    inport_test = inport;
    in_mask_test = in_mask;

    // Configure Input
    res = GPIO_SetDir(inport, in_mask, CSK_GPIO_DIR_INPUT);
    if (res) {
        CLI_LOG("configure Input fail\n");
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

int cli_gpio_in_get(char *params)
{
    uint32_t val;
    if (inport_test && in_mask_test) {
        val = GPIO_PinRead(inport_test, in_mask_test);
        CLI_LOG("input val %d\n", val);
    }
}

int cli_gpio_out(char *params)
{
    char *next = params, *token = NULL, *token_val = NULL;
    uint8_t output_port, output_pin, param_cnt;
    uint32_t output_val, val_set = 0;
    static int init = 0;
    void  *outport = NULL;
    uint32_t out_mask;
    int res = CLI_SUCCESS;

    while ((token = utils_next_token(&next)) && (token_val = utils_next_token(&next))) {
        if (!token || !token_val) {
            res = CLI_SHOW_USAGE;
            break;
        }

        if (!strcmp("port", token)) {
            output_port = atoi(token_val);
            param_cnt ++;
        }

        if (!strcmp("pin", token)) {
            output_pin = atoi(token_val);
            param_cnt ++;
        }

        if (!strcmp("val", token)) {
            output_val = atoi(token_val);
            val_set = 1;
            param_cnt ++;
        }
    }

    if (val_set)
        CLI_LOG("out port %d output pin %d output val %d \n", output_port, output_pin, output_val);
    else
        CLI_LOG("out port %d output pin %d \n", output_port, output_pin);
    if (output_port != CSK_IOMUX_PAD_A && output_port != CSK_IOMUX_PAD_B || \
        (output_port == CSK_IOMUX_PAD_A && output_pin > CSK_IOMUX_PAD_A_MAX_PIN) || \
        (output_port == CSK_IOMUX_PAD_B && output_pin > CSK_IOMUX_PAD_B_MAX_PIN) || \
        (param_cnt != 2 && param_cnt != 3)) {
        res = CLI_SHOW_USAGE;
    }

    /*if (((output_port == CSK_IOMUX_PAD_A) && (output_pin == 0 || output_pin == 1 || output_pin == 8 || output_pin == 9))) {
        CLI_LOG("gpioA, gpio pin should not use gpio 0/1/8/9 \n");
        res = CLI_SHOW_USAGE;
    }*/

    if (val_set && (output_val != 0 && output_val !=1)) {
        CLI_LOG("Not right output val\n");
        res = CLI_SHOW_USAGE;
    }

    if(res)
        return res;

    CLI_LOG("\r\n++++++++++ GPIO Out Test ++++++++++\r\n");

    // Initialize GPIO Controllers
    if (!init) {
        GPIO_Initialize(GPIOA(), NULL, NULL);
        GPIO_Initialize(GPIOB(), NULL, NULL);
        init = 1;
    }

    if (output_port == CSK_IOMUX_PAD_A) {
        outport = GPIOA();
        if (output_pin == 0 || output_pin == 1 || output_pin == 8 || output_pin == 9)
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, output_pin, CSK_IOMUX_FUNC_ALTER1);
        else
            IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, output_pin, CSK_IOMUX_FUNC_DEFAULT);
    } else {
        outport = GPIOB();
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, output_pin, CSK_IOMUX_FUNC_DEFAULT);
    }

    out_mask = (1UL << output_pin);
    outport_test = outport;
    out_mask_test = out_mask;

    // Configure Output
    GPIO_Control(outport, CSK_GPIO_DEBOUNCE_DISABLE, out_mask);
    res = GPIO_SetDir(outport, out_mask, CSK_GPIO_DIR_OUTPUT);
    if (res) {
        CLI_LOG("configure Output fail\n");
        return CLI_ERROR;
    }

    // configure output val
    if (val_set) {
        res = GPIO_PinWrite(outport, out_mask, output_val);
    }


    return CLI_SUCCESS;
}

int cli_gpio_out_write(char *params)
{
    char *next = params, *token = NULL;
    uint32_t val_set = 0, val;
    int res = CLI_SUCCESS;

    while (token = utils_next_token(&next)) {
        if (!token) {
            res = CLI_SHOW_USAGE;
            break;
        }

        val = atoi(token);
        val_set = 1;
    }

    if (val_set && (val != 0 && val !=1)) {
        CLI_LOG("Not right output val\n");
        res = CLI_SHOW_USAGE;
    }

    if(res)
        return res;

    if (outport_test && out_mask_test) {
        GPIO_PinWrite(outport_test, out_mask_test, val);
        CLI_LOG("output val %d\n", val);
    }
    return res;
}

