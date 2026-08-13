/**
 ****************************************************************************************
 *
 * @file cli_main.c
 *
 * @brief Cli cmd handle
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

#include "cli_main.h"
#ifdef CLI_TYPE_WF
#include "cli_wifi.h"
#include "cli_net.h"
#include "ls_misc.h"
#endif
#if CFG_MEMDUMP
#include "memdump.h"
#endif
#include "sdk_version.h"
#include "atcmd.h"
#ifdef CFG_AMP_IPC
#include "ipc.h"
#ifdef CFG_AMP_IPC_BUS_DEMO
#include "ipc_bus.h"
#endif
#ifdef CFG_IPC_TEST_CASE
#include "ipc_test.h"
#endif
#endif
#include "PSRAMManager.h"
#include "cache.h"
#include "flash_if.h"
#include "nvs_priv.h"
#include "nv_config.h"
#ifdef CLI_TYPE_WF
#include "wifi_api.h"
#endif
#if CONFIG_PM
#include "pm.h"
#include "vrtc.h"
#endif
#if CONFIG_DEEP_SLEEP
#include "PowerManager.h"
#endif

#if CFG_WIFI_MFG
extern ls_err_t wifi_mfg_exec(char *params, int32_t params_len);
#endif

static const struct cli_cmd cli_main_commands[];
extern void logDbg_enable_set(uint8_t logD_on_off);
cli_print_fn_t cli_print_func = logDbg;

#if CONFIG_GPIO_ADC_TEST
extern int cli_gpio_loop_test(char *params);
extern int cli_dac2adc_test(char *params);
extern int cli_adc2dac_test(char *params);
extern int cli_gpio_in(char *params);
extern int cli_gpio_in_get(char *params);
extern int cli_gpio_out(char *params);
extern int cli_gpio_out_write(char *params);
#endif

#ifdef CFG_ATCMD
extern void atcmd_handler(char* command, int len);
#endif
/**
 ****************************************************************************************
 * @brief Extract token from parameter list
 *
 * Extract the first parameter of the string. Parameters are separatd with space unless
 * it starts with " (or ') in which case it extract the token until " (or ') is reached.
 * " (or ') are then removed from the token.
 *
 * @param[in, out] params Pointer to parameters string to parse. Updated with remaining
 *                        parameters to parse.
 * @return pointer on first parameter
 ****************************************************************************************
 */
static char utils_sep_char;

#ifdef CLI_TYPE_WF

char *utils_next_token(char **params)
{
    char *str, *ptr = *params, *next = NULL;
    char sep  = ' ';

    utils_sep_char = sep;

    if (!ptr)
        return NULL;
    // ignore space
    while (*ptr == ' ')
        ptr++;
    if (ptr[0] == '\0')
        return NULL;

    if (ptr[0] == '"')
    {
        sep = ptr[0];
        utils_sep_char = sep;
        ptr++;
    }
    str = ptr;

    while (str && (next = strchr(str, sep)))
    {
        /*check '\\'*/
        if (*(next - 1) != '\\')
        {
            *next++ = '\0';
            while (*next == ' ')
                next++;
            if (*next == '\0')
                next = NULL;
            break;
        }
        else
        {
            if (*next++ == '\0')
                 next = NULL;
            str = next;
        }
    }
    *params = next;

    return ptr;
}

// for ' ' and '\'
size_t utils_get_proper_ssid_psk(char *token, uint8_t *str, uint8_t len)
{
    size_t i = 0;
    char pre_char = token[0], *next = token;
    bool esc, start_char = true;

    memset(str, 0, len);

    while (*next && (i < len))
    {
        esc = false;
        if (start_char)
        {
            if (*next != '\\')
            {
               str[i++] = *next;
            }
            start_char = false;
        }
        else if ((pre_char != '\\') && (*next != utils_sep_char) && (*next != '\\'))
        {
            str[i++] = *next;
        }
        else if ((pre_char == '\\') && ((*next == utils_sep_char) || (*next == '\\')))
        {
            str[i++] = *next;
            if ((pre_char == '\\') && (*next == '\\'))
                esc = true;
        }
        else
        {
            ;
        }

        pre_char = !esc ? *next : '\0';
        next++;
    }

    return i;
}

/**
 ****************************************************************************************
 * @brief Convert string containing MAC address
 *
 * The string may should be of the form xx:xx:xx:xx:xx:xx
 *
 * @param[in]  str   String to parse
 * @param[out] addr  Updated with MAC address
 * @return 0 if string contained what looks like a valid MAC address and -1 otherwise
 ****************************************************************************************
 */
int utils_parse_mac_addr(char *str, uint8_t *addr)
{
    char *ptr = str;
    uint32_t i;

    if (!str || (strlen(str) < 17) || !addr)
        return -1;

    for (i = 0; i < 6; i++)
    {
        char *next;
        long int hex = strtol(ptr, &next, 16);
        if (((unsigned)hex > 255) || ((hex == 0) && (next == ptr)) ||
            ((i < 5) && (*next != ':')) ||
            ((i == 5) && (*next != '\0')))
            return -1;

        addr[i] = (uint8_t)hex;
        ptr = ++next;
    }

    return 0;
}
/*
 ****************************************************************************************
 * @brief Convert string containing ip address
 *
 * The string may should be of the form a.b.c.d/e (/e being optional)
 *
 * @param[in]  str   String to parse
 * @param[out] ip    Updated with the numerical value of the ip address
 * @param[out] mask  Updated with the numerical value of the network mask
 *                   (or 32 if not present)
 * @return 0 if string contained what looks like a valid ip address and -1 otherwise
 ****************************************************************************************
 */
int utils_cli_parse_ip4(char *str, uint32_t *ip, uint32_t *mask)
{
    char *token;
    uint32_t a, i, j;

    #define check_is_num(_str)  for (j = 0; j < strlen(_str); j++) \
        {                                                          \
            if (_str[j] < '0' || _str[j] > '9')                    \
            return -1;                                             \
        }

    // Check if mask is present
    token = strchr(str, '/');
    if (token && mask)
    {
        *token++ = '\0';
        check_is_num(token);
        a = atoi(token);
        if ((a == 0) || (a > 32))
            return -1;
        *mask = (1 << a) - 1;
    }
    else if (mask)
    {
        *mask = 0xffffffff;
    }

    // parse the ip part
    *ip = 0;
    for (i = 0; i < 4; i++)
    {
        if (i < 3)
        {
            token = strchr(str, '.');
            if (!token)
                return -1;
            *token++ = '\0';
        }
        check_is_num(str);
        a = atoi(str);
        if (a > 255)
            return -1;
        str = token;
        *ip += (a << (i * 8));
    }

    return 0;
}

static int cli_memdump(char *param)
{
#if CFG_MEMDUMP
    CLI_LOG("uart dump begin, waiting to run script...\n");
    memdump_process(MDUMP_PATH_UART);

    return CLI_SUCCESS;
#else
    return CLI_UNKNOWN_CMD;
#endif
}

static int cli_get_chip_temp(char *param)
{
    int32_t temp = 0;
    temp = ls_get_cur_temp();
    CLI_LOG("get temperature %d \n", temp);

    return CLI_SUCCESS;

}

static int cli_temp_por_update(char *param)
{
    ls_temp_por_update();

    return CLI_SUCCESS;
}

static int cli_write_efuse(char *params)
{
    char *ptr = params, *next = params, *end;
    uint32_t addr, val, org_val;
    bool bit_or = false;

    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }
    addr = atoi(ptr);
    if(((addr != 11 && addr != 14 && addr != 15) && addr < 80) || addr > 127) {
        CLI_LOGE(": Addr:%d not in user defined region\n", addr);
        return CLI_SHOW_USAGE;
    }
    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }
    val = strtol(ptr, &end, 16);

    if ((ptr = utils_next_token(&next))) {
        if (!strcmp("-f", ptr))
            bit_or = true;
    }

   CLI_LOG("To write Efuse addr:%d val:0x%x\n", addr, val);

   if (ls_efuse_read_word(addr, &org_val)) {
       CLI_LOGE("Read efuse %d fail\n", addr);
       return CLI_ERROR;
   }

    if (org_val) {
        if (!bit_or) {
            CLI_LOGE(" efuse %d = 0x%x not empty, if still need to write, please add -f in cmd tail to force write , the write val would be (val | org_val) = 0x%x\n", addr, org_val, org_val|val);
            return CLI_ERROR;
        } else {
            val |= org_val;
            CLI_LOG("*** To write Efuse addr:%d new val:0x%x ***\n", addr, val);
        }
   }

    ls_efuse_write_word(addr, val);

    return CLI_SUCCESS;
}

static int cli_read_efuse(char *params)
{
    char *ptr = params, *next = params;
    uint32_t val = 0;
    uint8_t addr;
    int8_t ret;

    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }
    addr = (uint8_t)atoi(ptr);
    if (addr > 127) {
        CLI_LOGE(": Addr:%d not in user defined region\n", addr);
        return CLI_SHOW_USAGE;
    }

    ret = ls_efuse_read_word(addr, &val);
    if (ret)
        return CLI_ERROR;

    CLI_LOG("Efuse read addr:%d val:0x%x\n", addr, val);

    return CLI_SUCCESS;
}

static int cli_efuse_block_write(char *params)
{
    char *ptr = params, *next = params, *end;
    uint32_t addr, org_val[8], val[8], len;
    uint8_t *pos = NULL, i;
    bool bit_or = false;

    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }
    addr = atoi(ptr);
    if(((addr != 11 && addr != 14 && addr != 15) && addr < 80) || addr > 127) {
        CLI_LOGE(": Addr:%d not in user defined region\n", addr);
        return CLI_SHOW_USAGE;
    }

    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }
    len = (uint8_t)atoi(ptr);
    if (len > 8 || addr + len > 127) {
        CLI_LOGE(": exceed max block size (8 words) or addr + len > 127\n");
        return CLI_SHOW_USAGE;
    }

    CLI_LOG("To write Efuse addr:%d val:\n", addr);

    pos = (uint8_t *)val;
    for (i=0; i < len *4; i++) {
        if (!(ptr = utils_next_token(&next))) {
            CLI_LOGE(": expect %d input value,but only get %d \n", len*4, i);
            return CLI_SHOW_USAGE;
        }
        *pos = strtol(ptr, &end, 16);
         pos ++;
    }

    pos = (uint8_t *)val;
    for (i=0; i < len; i++) {
        pos = (uint8_t *)&val[i];
        CLI_LOG("0x%02x 0x%02lx 0x%02lx 0x%02lx ", *pos, *(pos + 1),  *(pos + 2),  *(pos + 3));
    }
    if ((ptr = utils_next_token(&next))) {
        if (!strcmp("-f", ptr))
            bit_or = true;
    }

    for (i = 0; i < len; i++) {
        if (ls_efuse_read_word(addr + i, &org_val[i])) {
            CLI_LOGE("Read efuse %d fail\n", addr);
            return CLI_ERROR;
        }
        if (org_val[i]) {
            if (!bit_or) {
                CLI_LOGE(" efuse %d = 0x%x not empty, if still need to write, please add -f in cmd tail to force write , the write val would be (val | org_val) = 0x%x\n", addr+i, org_val[i], org_val[i]|val[i]);
                return CLI_ERROR;
            } else {
                val[i] |= org_val[i];
                CLI_LOG("*** To write Efuse addr:%d new val:0x%x ***\n", addr + i, val[i]);
            }
        }
    }

    for (i = 0; i < len; i++) {
        ls_efuse_write_word(addr + i, val[i]);
    }

    return CLI_SUCCESS;
}

static int cli_efuse_block_read(char *params)
{
    char *ptr = params, *next = params;
    uint8_t addr, len, i, val[4];
    int8_t ret;

    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }
    addr = (uint8_t)atoi(ptr);
    if (addr > 127) {
        CLI_LOGE(": Addr:%d not in user defined region\n", addr);
        return CLI_SHOW_USAGE;
    }
    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }

    len = (uint8_t)atoi(ptr);
    if (len > 8 || addr + len > 127) {
        CLI_LOGE(": exceed max block size (8 words) or addr + len > 127 \n");
        return CLI_SHOW_USAGE;
    }

    CLI_LOG("Efuse read addr:%d val:\n", addr);

    for (i = 0; i < len; i++) {
        ret = ls_efuse_read_word(addr + i, (uint32_t *)val);
        if (ret)
        return CLI_ERROR;
        CLI_LOG("0x%02lx 0x%02lx 0x%02lx 0x%02lx \n", val[0], val[1], val[2], val[3] );
    }

    return CLI_SUCCESS;
}


#define DEFAULT_LINE_LENGTH_BYTES    16
/*
 * Maximum length of an output line is when width == 1
 *  9 for address,
 *  a space, two hex digits
 *  \0 terminator
 */
#define HEXDUMP_MAX_BUF_LENGTH(bytes)   (9 + (bytes) * 3 + 1)

static int32_t cli_mem_hexdump_line(int32_t addr, int32_t width, int32_t count,
         uint32_t linelen, char *out, int32_t size)
{
    uint32_t thislinelen, hex;
    int32_t i;

    out += sprintf(out, "%08lx:", addr);
    thislinelen = (count < linelen)? count : linelen;

    for (i = 0; i < thislinelen; i++)
    {
        if (width == sizeof(uint32_t))
            hex = *(volatile uint32_t *)addr;
        else if (width == sizeof(uint16_t))
            hex = *(volatile uint16_t *)addr;
        else
            hex = *(volatile uint8_t *)addr;
        out  += sprintf(out, " %0*x", (unsigned int)(width*2), (unsigned int)hex);
        addr += width;
    }
    *out = 0;

    return thislinelen;
}

int32_t cli_mem_dump(uint32_t addr, int32_t width, uint32_t count, uint32_t linelen)
{
    int32_t thislinelen;
    char buf[HEXDUMP_MAX_BUF_LENGTH(width * linelen)];

    if (width > sizeof(uint8_t))
        count = (count + (width - 1))/width;
    if (count == 0)
        count = 1;
    linelen = linelen / width;
    while (count)
    {
        thislinelen = cli_mem_hexdump_line(addr, width, count, linelen,
                       buf, sizeof(buf));
        CLI_LOGI("%s\r\n", buf);

        /* update references */
        addr  += thislinelen * width;
        count -= thislinelen;
    }

    return 0;
}

static int32_t cli_mem_write(uint32_t addr, int32_t width, uint32_t value)
{
    if (width == sizeof(uint32_t))
        *(volatile uint32_t *)addr = value;
    else if (width == sizeof(uint16_t))
        *(volatile uint16_t *)addr = value & 0xFFFF;
    else
        *(volatile uint8_t *)addr = value & 0xFF;

    cli_mem_dump(addr, width, width, DEFAULT_LINE_LENGTH_BYTES);

    return 0;
}

static int cli_mem(char *params)
{
    uint32_t addr, rw, value = 0;
    int32_t width = sizeof(uint32_t);
    char *ptr = params, *next = params;

    if (!(ptr = utils_next_token(&next)))
        return CLI_ERROR;

    if (ptr[0] == '-')
    {
        switch (ptr[1])
        {
            case 'l':
                width = sizeof(uint32_t);
                break;
            case 'b':
                width = sizeof(uint8_t);
                break;
            case 'w':
                width = sizeof(uint16_t);
                break;
            default:
                return CLI_ERROR;
        }
        if (!(ptr = utils_next_token(&next)))
            return CLI_ERROR;
    }

    if (*ptr == 'r')
        rw = 0;
    else if (*ptr == 'w')
        rw = 1;
    else
        return -1;

    if ((ptr = utils_next_token(&next)))
    {
        addr = strtoul(ptr, NULL, 0);
        if (addr > 0)
        {
            if ((ptr = utils_next_token(&next)))
                value = strtoul(ptr, NULL, 0);

            if (rw == 0)
                cli_mem_dump(addr, width, value, DEFAULT_LINE_LENGTH_BYTES);
            else
                cli_mem_write(addr, width, value);
        }
    }

    return CLI_SUCCESS;
}

static int cli_log_enable(char *params)
{
    char *ptr = params, *next = params;
    uint8_t val = 0;

    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }
    val = (uint8_t)atoi(ptr);
    logDbg_enable_set(!val);

    return CLI_SUCCESS;
}

static int cli_log_level_set(char *params)
{
    char *ptr = params, *next = params;
    uint8_t val = 0;

    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }
    val = (uint8_t)atoi(ptr);
    if (val < CLOG_LEVEL_NONE || val > CLOG_LEVEL_VERBOSE)
    {
        return CLI_SHOW_USAGE;
    }
    cloglvl = val;

    return CLI_SUCCESS;
}

static int cli_rtos_info(char *params)
{
    int total, used, free, max_used;
    uint8_t *task_info;

    rtos_heap_info(&total, &free, &max_used);
    used = total - free;
    max_used = total - max_used;

    CLI_LOGI("RTOS HEAP:free=%d used=%d max_used=%d/%d\n", free, used, max_used, total);

    task_info = rtos_malloc(512);
    if (task_info)
    {
        vTaskList(task_info);
        CLI_LOGI("task_info(len:%d):\n%s\n%", strlen((char *)task_info),task_info);
        rtos_get_cpu_usage((char *)task_info, 512);
        CLI_LOGI("\nCPU usage:\n%s\t\t%s\t\t%s\n%s", "Task", "Time", "%CPU", task_info);
        /*memset(task_info, 0 , 512);
        rtos_get_cpu_usage1(task_info, 512);
        CLI_LOGI("\nCPU usage:\n%s\t\t%s\t\t%s\n%s", "Task", "Time", "%CPU", task_info);
        */
        rtos_free(task_info);
    }

    return CLI_SUCCESS;
}

#define VERSION_STR_SIZE 256
int cli_version(char *params)
{
    uint8_t *version;

    version = rtos_aligned_malloc(VERSION_STR_SIZE, HAL_DCACHE_CFG_LINE_SIZE);
#ifdef SDK_VER_ON
    CLI_LOG("  SDK Version:\n    Ver v%s - build: %s %s\n    Commit ID:%s",
        SDK_BUILD_VER, SDK_BUILD_USER, SDK_BUILD_DATE,
        SDK_COMMIT_ID);
#endif

    if (!version)
        return CLI_ERROR;
    memset(version, 0, VERSION_STR_SIZE);
    wifi_ls_mac_version(version, VERSION_STR_SIZE);
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    if (((uint32_t)version >= PSRAM_BASE_ADDRESS) && DCachePresent())
    {
        vPortEnterCritical();
        HAL_InvalidateDCache_by_Addr((uint32_t *)version, VERSION_STR_SIZE);
        vPortExitCritical();
    }
#endif
    CLI_LOG("%s", version);
    rtos_aligned_free(version);

    return CLI_SUCCESS;
}
#if CONFIG_PM
static int cli_light_sleep(char *params)
{
    char *ptr = NULL, *next = params;
    uint32_t val, level = 0;
    pm_config_t config = {.mode = PM_MODE_LIGHT_SLEEP};
    pm_sleep_config_t sleep_config;

    if (!(ptr = utils_next_token(&next)))
        goto END;

    if (!strcmp(ptr, "off"))
    {
        config.mode = PM_MODE_ACTIVE;
        goto END;
    }

    memset(&sleep_config, 0, sizeof(pm_sleep_config_t));
    while (ptr != NULL)
    {
        if (ptr[0] == '-')
        {
            switch (ptr[1])
            {
                case ('f'):
                    if (!(ptr = utils_next_token(&next)))
                        return CLI_SHOW_USAGE;
                    if (!strcmp(ptr, "0"))
                        config.auto_mode = false;
                    else
                        config.auto_mode = true;
                    break;
                case ('d'):
                    if (!(ptr = utils_next_token(&next)))
                        return CLI_SHOW_USAGE;
                    val = atoi(ptr);
                    #if CONFIG_PM_DEBUG
                    if (val < PM_DBG_MAX)
                        config.dbg_level = val;
                    #endif
                    break;
                case ('g'):
                    if (!(ptr = utils_next_token(&next)))
                        return CLI_SHOW_USAGE;
                    val = atoi(ptr);
                    if (val < PM_GPIO_PIN_MAX)
                    {
                        sleep_config.wakeup_src_mask = 1 << PM_WAKEUP_GPIO;
                        sleep_config.gpio_mask = 1 << val;
                    }
                    break;
                case ('l'):
                    if (!(ptr = utils_next_token(&next)))
                        return CLI_SHOW_USAGE;
                    level = atoi(ptr);
                    break;
                case ('p'):
                    if (!(ptr = utils_next_token(&next)))
                        return CLI_SHOW_USAGE;
                    config.clock_level = (uint8_t)atoi(ptr);
                    break;
                default:
                    return CLI_SHOW_USAGE;
            }
        }
        ptr = utils_next_token(&next);
    };

    if (sleep_config.gpio_mask != 0)
        if (level)
            sleep_config.gpio_level = sleep_config.gpio_mask;

    pm_set_sleep_config(&sleep_config);
END:
    pm_set_config(&config);

    return CLI_SUCCESS;
}
#endif
#if CONFIG_DEEP_SLEEP
static int cli_deep_sleep(char *params)
{
    char *ptr = NULL, *next = params;
    uint32_t val;

    while ((ptr = utils_next_token(&next)))
    {
        if (ptr[0] == '-')
        {
            switch (ptr[1])
            {
                case ('g'):
                    if (!(ptr = utils_next_token(&next)))
                        return CLI_SHOW_USAGE;
                    break;
                default:
                    return CLI_SHOW_USAGE;
            }
        }
    };
    vPortEnterCritical();
    #if (BOOT_HARTID == 0)
    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
    #else
    HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_CP);
    #endif

    log_flush();
    HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI);
    vPortExitCritical();
    logDbg("Failed to enter deep sleep\n");
    while (1);
}
#endif
#ifdef CFG_AMP_IPC
static int cli_ipc_dump(char *params)
{
    ipc_stats_dump();

    return 0;
}

#ifdef CFG_AMP_IPC_BUS_DEMO
int32_t ipc_bus_demo_ping(uint32_t value);
int32_t ipc_bus_demo_post(uint32_t value);
int32_t ipc_bus_demo_publish(uint32_t value);

static int cli_ipc_bus_test(char *params)
{
    char *token;
    char *value_token;
    char *next = params;
    uint32_t value = 1;
    int32_t ret = 0;

    token = utils_next_token(&next);
    if (token == NULL)
        return CLI_SHOW_USAGE;

    value_token = utils_next_token(&next);
    if (value_token != NULL)
        value = strtoul(value_token, NULL, 0);

    if (!strcmp(token, "ping"))
    {
        ret = ipc_bus_demo_ping(value);
    }
    else if (!strcmp(token, "post"))
    {
        ret = ipc_bus_demo_post(value);
    }
    else if (!strcmp(token, "publish"))
    {
        ret = ipc_bus_demo_publish(value);
    }
    else if (!strcmp(token, "all"))
    {
        ret = ipc_bus_demo_ping(value);
        if (ret == 0)
            ret = ipc_bus_demo_post(value);
        if (ret == 0)
            ret = ipc_bus_demo_publish(value);
    }
    else
    {
        return CLI_SHOW_USAGE;
    }

    if (ret != 0)
    {
        CLI_LOGE("ipc_bus %s failed ret=%d\n", token, ret);
        return CLI_ERROR;
    }

    CLI_LOG("ipc_bus %s ok\n", token);
    return CLI_SUCCESS;
}
#endif

#ifdef CFG_IPC_TEST_CASE
int cli_ipc_test(char *params)
{
    char *token, *next = params;
    struct ipc_test_param param = {0};

    token = utils_next_token(&next);
    if (token == NULL)
        return CLI_SHOW_USAGE;
    param.task_pairs = 1;
    if (strcmp("stop", token))
    {
        do
        {
            if (token[0] == '-')
            {
                switch (token[1])
                {
                    case 'c':
                        token = utils_next_token(&next);
                        if (!token)
                            return CLI_SHOW_USAGE;
                        param.cnt = atoi(token);
                        break;
                    case 'p':
                        token = utils_next_token(&next);
                        if (!token)
                            return CLI_SHOW_USAGE;
                        param.task_pairs = atoi(token);
                        if (param.task_pairs > 4)
                            param.task_pairs = 4;
                        break;
                    default:
                        break;
                }
            }
        } while((token = utils_next_token(&next)));

        ipc_test_start(&param);
    }
    else
    {
        ipc_test_stop();
    }

    return CLI_SUCCESS;
}

int cli_ipc_slave_test(char *params)
{
    char *token, *next = params;
    struct ipc_test_param param = {0};

    token = utils_next_token(&next);
    if (token == NULL)
        return CLI_SHOW_USAGE;
    param.task_pairs = 1;
    if (strcmp("stop", token))
    {
        do
        {
            if (token[0] == '-')
            {
                switch (token[1])
                {
                    case 'c':
                        token = utils_next_token(&next);
                        if (!token)
                            return CLI_SHOW_USAGE;
                        param.cnt = atoi(token);
                        break;
                    case 'p':
                        token = utils_next_token(&next);
                        if (!token)
                            return CLI_SHOW_USAGE;
                        param.task_pairs = atoi(token);
                        if (param.task_pairs > 4)
                            param.task_pairs = 4;
                        break;
                    default:
                        break;
                }
            }
        } while((token = utils_next_token(&next)));

        ipc_test_start_slave(&param);
    }
    else
    {
        ipc_test_stop_slave();
    }

    return CLI_SUCCESS;
}
#endif
#ifdef CFG_IPC_PRINT_READER
extern void ipc_dbg_enable(int32_t enable);
int cli_ipc_dbg(char *params)
{
    char *token, *next = params;

    token = utils_next_token(&next);
    if (token == NULL)
        return CLI_SHOW_USAGE;
    if (!strcmp("on", token))
    {
         ipc_dbg_enable(1);
    }
    else if (!strcmp("off", token))
    {
        ipc_dbg_enable(0);
    }

    return CLI_SUCCESS;
}
#endif
#endif
static int cli_help(char *params)
{
    uint8_t i = 0;
    char buf[] = "wifi?";
    char bt_help[] = "bt?";

    for (; cli_main_commands[i].exec != NULL; i++)
    {
        CLI_LOG(" - %s %s\n", cli_main_commands[i].name, cli_main_commands[i].params);
    }
    /* wifi cli cmd */

#if defined(CLI_TYPE_WF)
    wifi_cmd_handler(buf, 6);
#endif

#if defined(CLI_TYPE_BT)
    bt_cmd_handler(bt_help, 4);
#endif
    return CLI_SUCCESS;
}

#if CFG_WIFI_MFG
static int wifi_cli_mfg(char *params)
{
    if (params)
        wifi_mfg_exec(params, strlen(params) + 1);

    return CLI_SUCCESS;
}
#endif

#if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
static int8_t flash_if_erase_otp(void)
{
    int8_t ret = 0;
    off_t offset = 0;

#if USE_FLASH_OTP == 1
    if (!flash_if_check_security_support()) {
        CLI_LOGW("Flash unsupport OTP region!\n");
        return -1;
    }
#endif
    flash_if_write_protection_set(false);
#if USE_FLASH_OTP == 1
    ret = flash_if_security_erase(offset);
#else
    offset = FLASH_NOR_OTP_NV_BASE_ADDR;
    ret = flash_if_erase(offset, FLASH_OTP_NV_LENGTH);
    CLI_LOGI("erase flash otp offset %x length %x\n", offset, FLASH_OTP_NV_LENGTH);
#endif
    flash_if_write_protection_set(true);
    if (ret) {
        CLI_LOGW("erase flash OTP failed, ret=%d\n", ret);
        return -1;
    }
    else {
        CLI_LOGI("erase flash OTP success\n");
    }

    return ret;
}

static int8_t flash_if_set_otp_flag(uint32_t magic_code)
{
    int32_t ret = 0;
    off_t offset = 0;
    uint32_t write_val = magic_code;
    uint32_t read_val = 0;

#if USE_FLASH_OTP == 1
    if (!flash_if_check_security_support()) {
        CLI_LOGE("Flash unsupport OTP region!\n");
        return -1;
    }
#else
    offset = FLASH_NOR_OTP_NV_BASE_ADDR;
#endif
    flash_if_write_protection_set(false);
#if USE_FLASH_OTP == 1
    ret = flash_if_security_erase(offset);
#else
    ret = flash_if_erase(offset, sizeof(write_val));
#endif
    if (ret) {
        CLI_LOGE("erase flash OTP failed, ret=%d\n", ret);
        goto write_failed;
    }
    else {
        CLI_LOGI("erase flash OTP success\n");
    }
#if USE_FLASH_OTP == 1
    ret = flash_if_security_write(offset, (void *)&write_val, sizeof(write_val));
#else
    ret = flash_if_write(offset, (void *)&write_val, sizeof(write_val));
#endif
    if (ret) {
        CLI_LOGE("write Flash OTP flag failed, ret=%d\n", ret);
        goto write_failed;
    } else
        CLI_LOGI("write Flash OTP flag success\n");
#if USE_FLASH_OTP == 1
    ret = flash_if_security_read(0, &read_val, sizeof(read_val));
#else
    ret = flash_if_read(offset, (void *)&read_val, sizeof(read_val));
#endif
    if (ret < 0 || read_val != magic_code)
    {
        CLI_LOGE("read otp failed or invalid value(%x), ret %d\n", read_val, ret);
        goto write_failed;
    }
    flash_if_write_protection_set(true);
    return 0;
write_failed:
    flash_if_write_protection_set(true);
    return -1;
}

static int cli_clear_otp(char *params)
{
#ifdef CFG_FLASH_IF
    int8_t ret = 0;
    uint32_t read_val = 0;

    ret = flash_if_erase_otp();
    if (ret < 0)
    {
        CLI_LOGE("clear otp failed, ret %d\n", ret);
        return CLI_ERROR;
    }
#if USE_FLASH_OTP == 1
    ret = flash_if_security_read(0, &read_val, sizeof(read_val));
#else
    ret = flash_if_read(FLASH_NOR_OTP_NV_BASE_ADDR, &read_val, sizeof(read_val));
#endif
    if (ret < 0 || read_val != 0xffffffff)
    {
        CLI_LOGE("read otp failed or invalid value(%x), ret %d\n", read_val, ret);
        return CLI_ERROR;
    }
    CLI_LOGI("clear otp success, read back value %x\n", read_val);
#endif
    return CLI_SUCCESS;
}

static int cli_set_otp_flag(char *params)
{
#ifdef CFG_FLASH_IF
    char *ptr = params, *next = params;
    uint32_t val = 0;
    int8_t ret = 0;

    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }
    val = strtoul(ptr, NULL, 0);
    ret = flash_if_set_otp_flag(val);
    if (ret < 0)
    {
        CLI_LOGE("set otp flag failed, ret %d\n", ret);
        return CLI_ERROR;
    }
    CLI_LOGI("set otp flag 0x%08x\n", val);
#endif
    return CLI_SUCCESS;
}

static int cli_check_otp(char *params)
{
#if CFG_FLASH_IF
    int32_t ret = 0;
    uint32_t calc_crc = 0;
    ls_nv_fixzone_header_t *hdr;
    ls_nv_selfcali_cfg_t *otp_config = rtos_malloc(sizeof(ls_nv_selfcali_cfg_t));

    if (!otp_config) {
        CLI_LOGE("malloc otp_config failed\n");
        return CLI_ERROR;
    }
    hdr = &otp_config->hdr;

#if USE_FLASH_OTP == 1
    if (!flash_if_check_security_support()) {
        CLI_LOGE("Flash unsupport OTP region!\n");
        goto failed;
    }
    ret = flash_if_security_read(0, otp_config, sizeof(*otp_config));
#else
    ret = flash_if_read(FLASH_NOR_OTP_NV_BASE_ADDR, otp_config, sizeof(*otp_config));
#endif
    if (ret) {
        CLI_LOGW("Read otp config failed, ret=%d\n", ret);
        goto failed;
    } else {
        CLI_LOGI("Read otp config success\n");
    }
    CLI_LOGI("Otp check magic %x len %x version %x crc32 %x\n", hdr->magic, hdr->length, hdr->version, hdr->crc32);
    if (hdr->magic != NV_MAGIC_PATTERN2) {
        CLI_LOGI("NV fix zone magic (%x) mismatch, skip it!\n", hdr->magic);
        goto failed;
    }
    calc_crc = crc32(calc_crc, (uint8_t *)(hdr), (sizeof(*hdr) - 4));
    calc_crc = crc32(calc_crc, (uint8_t *)(hdr + 1), hdr->length);
    if (calc_crc != hdr->crc32) {
        CLI_LOGI("NV fix zone crc (%x) check failed, expect (%x) skip it!\n", hdr->crc32, calc_crc);
        goto failed;
    }
    free(otp_config);
    CLI_LOGI("Check otp success\n");
    return CLI_SUCCESS;
failed:
    free(otp_config);
    CLI_LOGE("Check otp failed\n");
    return CLI_ERROR;
#else
    CLI_LOGE("unsupport flash_if module!\n");
    return CLI_ERROR;
#endif
}

static int cli_cali_redo(char *params)
{
    ls_rf_cali_redo(1);
    return CLI_SUCCESS;
}

static int cli_dpd_redo(char *params)
{
    ls_rf_cali_redo(0);
    return CLI_SUCCESS;
}
#endif

int cli_dpd_track(char *params)
{
    char *token, *next = params;
    uint8_t enable = 0;

    token = utils_next_token(&next);
    if (token == NULL)
        return CLI_SHOW_USAGE;

    enable = atoi(token);
    if (enable >= 0 && enable < 4)
        wifi_dpd_track_connect_switch(enable);
    else
        return CLI_SHOW_USAGE;

    return CLI_SUCCESS;
}

static int cli_set_temp_thr(char *params)
{
    char *token, *next = params;
    uint32_t thr = 0;

    token = utils_next_token(&next);
    if (token == NULL)
        return CLI_SHOW_USAGE;
    thr = (uint32_t)strtoul(token, NULL, 0);
    ls_set_temp_thr(thr);
    return CLI_SUCCESS;
}

#ifdef CFG_MQTT_TEST
extern void mqtt_test_start(char* host);

static int cli_mqtt_test(char *params)
{
    char *token, *next = params;
    uint32_t ip = 0;
    int32_t ret = CLI_SUCCESS;

    if (next)
    {
        token = utils_next_token(&next);
        if (!strcmp(token, "stop"))
            mqtt_test_start(NULL);
        else
            mqtt_test_start(token);
    }
    else
    {
        ret = CLI_SHOW_USAGE;
    }

    return ret;
}
#endif

static const struct cli_cmd cli_main_commands[] =
{
    {cli_help, "help", ""},
    {cli_help, "?", ""},
    {cli_version,     "version", "show version information"},
    {net_cli_reboot, "reset", ""},
#ifndef WIFI_RAM_ATE
#if CFG_PING
    {net_cli_ping, "ping",
     "[-s <pkt_size>] [-r <rate>] [-d (duration)] [-Q <ToS>] [-G (background)] <dest_ip>\n"
     "     stop <id> [-t (continuous)]"},
#endif
#if CFG_IPERF
    {net_cli_iperf, "iperf", "-s|-c <host>|-h [options (use -h for details)]"},
#endif
    {net_cli_sigkill, "sigkill", "<cmd_id>"},
#if CFG_MEMDUMP
    {cli_memdump,   "memdump",            "dump memory through uart"},
#endif
#ifdef CFG_AMP_IPC
    {cli_ipc_dump, "ipc_dump", ""},
#ifdef CFG_AMP_IPC_BUS_DEMO
    {cli_ipc_bus_test, "ipc_bus", "ping [value]|post [value]|publish [value]|all [value]"},
#endif
#ifdef CFG_IPC_TEST_CASE
     {cli_ipc_test, "ipc_test", "[-p task_pairs] [-c count]\n    stop"},
     {cli_ipc_slave_test, "ipc_slave_test", "[-p task_pairs] [-c count]\n    stop"},
#endif
#ifdef CFG_IPC_PRINT_READER
     {cli_ipc_dbg, "ipc_dbg", "[on|off]\n"},
#endif
#endif
    {cli_log_enable,     "log", "<value> 0: off, 1: on"},
    {cli_log_level_set,     "log_level", "<level> 0~5 : NONE/ERR/WARN/INFO/DEBUG/VERBOSE"},
    {cli_rtos_info,     "rtos_info", "show rtos mem usage and task info"},
#if defined(RF_SELF_CALI_WRITE_TO_NV) || defined(RF_SELF_CALI_FROM_NV)
    {cli_clear_otp,     "clear_otp", "clear the otp region"},
    {cli_set_otp_flag,  "set_otp_flag", "[flag] set flag 0x11223344 mean to burn OTP"},
    {cli_check_otp,     "check_otp", "check the otp region valid or not"},
    {cli_cali_redo,  "redo_cali", "redo rf ppa cap + dpd calibration, should in wifi disconnect or soft ap stop state"},
    {cli_dpd_redo,  "redo_dpd", "redo rf dpd calibration, should in wifi disconnect or soft ap stop state"},
#endif
    {cli_dpd_track,  "dpd_track", "<value>  \n"
     "            0: disable dpd on connect time and temp comp \n"
     "            1: enable dpd on connect time and disable dpd on temp comp \n"
     "            2: disable dpd on connect time and enable dpd on temp comp \n"
     "            3: enable dpd on connect time and enable dpd on temp comp \n" },
#if CONFIG_PM
    {cli_light_sleep,    "light_sleep",  "[off] [-t <timer value(ms)>] [-r <retention bits>] [-g <gpio pin number>]\n"},
#endif
#endif
#if CONFIG_DEEP_SLEEP
    {cli_deep_sleep,    "deep_sleep", ""},
#endif
    {cli_mem,    "mem", "[-b] [-w] [-l] r/w addr [len/value]"},
#if CFG_WIFI_MFG
    {wifi_cli_mfg, "mfg", "config|set|start|stop "
    "[-c <freq>] [-m <mcs idx>] [-s <b|g|n|ax>] [-l <payload length>] [-h] [options (use -h for details)]" },
#endif
#ifndef CFG_AMP_IPC
    {cli_get_chip_temp,    "temp",		 "get chip temperature"},
    {cli_temp_por_update,    "temp_por",	   "temp por update for testing"},
    {cli_set_temp_thr,    "temp_thr",	   "[temp_degree] 10-60℃ set temperature compensation threshold for por update \r\n"},
    {cli_write_efuse,    "efuse_write",	 "<addr in word (80-127) (decimal)> <value in hex> [-f (optional)] \r\n"
    "            [-f]: if the efuse is not empty, and still need to write, should add -f in cmd tail. the new value = value | original val \r\n"},
    {cli_read_efuse,    "efuse_read",		"<addr in word (0-127) (decimal)>\r\n"
    "            Tips: efuse read/write unit is one word (4 bytes), addr 80 means efuse byte offset 320, addr 81 means efuse byte offset 324 \r\n"},
    {cli_efuse_block_write,    "efuse_block_write",	 "<addr in word (80-127) (decimal)> <len (length of words, decimal, max 8)> <value0 (1byte hex)> <value1> ... <value>" "[-f (optional)]\r\n"
    "            [-f]: if the efuse is not empty, and still need to write, should add -f in cmd tail. the new value = value | original val\r\n"},
    {cli_efuse_block_read,    "efuse_block_read",    "<addr in word (0-127) (decimal)>" "<len (length of words, decimal, max 8)>\r\n"},
#endif
#if CONFIG_GPIO_ADC_TEST
    {cli_gpio_loop_test,    "gpio_loop_test",  "[in_port] <val>" "[in_pin] <val>" "[out_port] <val>" "[out_pin] <val> \r\n"
    "            gpio port val 0: GPIOA 1:GPIOB> pin val GPIOA 0~31 (in some case GPIO A 0/1/8/9 would be occupied by jtag), GPIOB 0~9 \r\n"
    "            example: gpio_test in_port 0 in_pin 4 out_port 0 out_pin 5"},
    {cli_gpio_in,    "gpio_in_cfg",  "[port] <val>" "[pin] <val>"  "\r\n"
    "            gpio port val 0: GPIOA 1:GPIOB> pin val GPIOA 0~31 (in some case GPIO A 0/1/8/9 would be occupied by jtag), GPIOB 0~9 \r\n"
    "            example: gpio_in_cfg port 0 pin 4"},
    {cli_gpio_in_get,     "gpio_in_get",    "get gpio input value \r\n"
    "            example: 1. config gpio pin as input: gpio_in_cfg port 0 pin 4 \r\n"
    "                     2. after input signal to this pin, get gpio input val: gpio_in_get \r\n"},
    {cli_gpio_out,    "gpio_out_cfg",  "[port] <val>" "[pin] <val>"  "[val] <0/1>\r\n"
    "            gpio port val 0: GPIOA 1:GPIOB> pin val GPIOA 0~31 (in some case GPIO A 0/1/8/9 would be occupied by jtag), GPIOB 0~9 \r\n"
    "            example: gpio_out_cfg port 0 pin 5"},
    {cli_gpio_out_write,     "gpio_output",    "<val 0/1 gpio output value> \r\n"
    "            example: 1. config gpio pin as output: gpio_out_cfg port 0 pin 5 val 1 \r\n"
    "                     2. set output val again: gpio_output 0 \r\n"},
    {cli_dac2adc_test,     "dac2adc_test",    "<val (0:adc0 1:adc1)>  <delay (option, unit ms, defaut 1000ms)>"
     "           dac2adc loop test dac <---> adc0 or dac <---> adc1 \r\n"},
    {cli_adc2dac_test,     "adc2dac_test",    "<val (0:adc0 1:adc1)> "
     "           adc2dac loop test adc0 <---> dac or adc1 <---> dac \r\n"},
#endif
    /* could add other cli cmd below */
#ifdef CFG_MQTT_TEST
    {cli_mqtt_test,    "mqtt_test",  "<ip>"},
#endif
    {NULL, "", ""}
};
#endif

uint32_t cli_cmd_handler(char* command, int len)
{
    uint32_t res;
    char *param;
    const struct cli_cmd *cmd;

    param = strchr(command, ' ');
    if (param)
    {
        *param++ = '\0';
        while (*param == ' ')
            param++;
    }
    else
    {
        /* be sure to have \0 in command */
        command[len - 1] = '\0';
    }

    cmd = cli_main_commands;
    while (cmd->exec)
    {
        if (!strcmp(command, cmd->name))
            break;
        cmd++;
    }

    if (cmd->exec)
    {
        res = (uint32_t)cmd->exec(param);
        /* Add default response */
        if (res == CLI_SHOW_USAGE)
        {
            CLI_LOG("Usage:\n%s %s\n",
                        cmd->name, cmd->params);
        }
    }
    else
    {
        res = CLI_UNKNOWN_CMD;
    }

    return res;
}

void set_cli_print_func(cli_print_fn_t new_func)
{
    cli_print_func = new_func;
}

int32_t cli_shell_process(char *command, int32_t len, int32_t (*func)(uint8_t*, int32_t))
{
    uint32_t res = CLI_UNKNOWN_CMD;

    if (!memcmp(command, "wifi", 4))
    {
#ifdef CLI_TYPE_WF
        res = wifi_cmd_handler(command, len);
#endif
    }
#ifndef WIFI_RAM_ATE
    else if (!memcmp(command, "bt", 2) || !memcmp(command, "ble", 3))
    {
#ifdef CLI_TYPE_BT
        res = bt_cmd_handler(command, len);
#endif
    }
#ifdef CFG_ATCMD
    else if (!memcmp(command, "AT", 2))
    {
        atcmd_handler(command, len);
        res = CLI_SUCCESS;
    }
#endif
#endif
    else
    {
        res = cli_cmd_handler(command, len);
    }
    if (res == CLI_UNKNOWN_CMD)
    {
        CLI_LOG("UNKNOWN CMD \r\n");
    }
    return 0;
}
/**
 * @}
 */
