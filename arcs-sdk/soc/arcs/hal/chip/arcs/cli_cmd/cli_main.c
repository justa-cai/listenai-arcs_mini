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
#include "cli_wifi.h"
#include "cli_net.h"
#include "ls_misc.h"
#if CFG_MEMDUMP
#include "memdump.h"
#endif
#include "sdk_version.h"
#include "atcmd.h"
#ifdef CFG_AMP_IPC
#include "ipc.h"
#ifdef IPC_TEST_CASE
#include "ipc_test.h"
#endif
#endif
#include "PSRAMManager.h"
#include "cache.h"
#include "flash_if.h"
#include "nvs_priv.h"
#include "nv_config.h"
#if CFG_WIFI_MFG
ls_err_t wifi_mfg_exec(char * params, int32_t params_len);
#endif

static const struct cli_cmd cli_main_commands[];
extern void logDbg_enable_set(uint8_t logD_on_off);
extern int wifi_ls_mac_version(char *ver, int size);
cli_print_fn_t cli_print_func = logDbg;
ls_nv_selfcali_cfg_t otp_config = {0};
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

static int cli_memdump(void *param)
{
#if CFG_MEMDUMP
    CLI_LOG("uart dump begin, waiting to run script...\r\n");
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
    CLI_LOG("get temperature %d \r\n", temp);

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
    uint32_t addr, val;

    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }
    addr = atoi(ptr);
    if(((addr != 11 && addr != 14 && addr != 15) && addr < 64) || addr > 127) {
        CLI_LOGE(": Addr:%d not in user defined region\r\n", addr);
        return CLI_SHOW_USAGE;
    }
    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }
    val = strtol(ptr, &end, 16);

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
        CLI_LOGE(": Addr:%d not in user defined region\r\n", addr);
        return CLI_SHOW_USAGE;
    }

    ret = ls_efuse_read_word(addr, &val);
    if (ret)
        return CLI_ERROR;

    CLI_LOGI(": Efuse read addr:%d val:0x%x\r\n", addr, val);

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
        CLI_LOG("%s\r\n", buf);

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
    // char *ptr = params, *next = params;
    // uint8_t val = 0;

    // if (!(ptr = utils_next_token(&next))) {
    //     return CLI_SHOW_USAGE;
    // }
    // val = (uint8_t)atoi(ptr);
    // logDbg_enable_set(!val);

    return CLI_ERROR;
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

    CLI_LOGI("RTOS HEAP:free=%d used=%d max_used=%d/%d\r\n", free, used, max_used, total);

    task_info = rtos_malloc(512);
    if (task_info)
    {
        vTaskList(task_info);
        CLI_LOGI("task_info(len:%d):\r\n%s\r\n%", strlen(task_info),task_info);
        rtos_get_cpu_usage(task_info, 512);
        CLI_LOGI("\nCPU usage:\n%s\t\t%s\t\t%s\r\n%s", "Task", "Time", "%CPU", task_info);
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
    CLOG("  SDK Version:\n    Ver v%s - build: %s %s\n    Commit ID:%s",
        SDK_BUILD_VER, SDK_BUILD_USER, SDK_BUILD_DATE,
        SDK_COMMIT_ID);
#endif

    if (!version)
        return CLI_ERROR;
    memset(version, 0, VERSION_STR_SIZE);
#if defined(CLI_TYPE_WF)
    wifi_ls_mac_version(version, VERSION_STR_SIZE);
#endif
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    if ((version >= PSRAM_BASE_ADDRESS) && DCachePresent())
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

#ifdef CFG_AMP_IPC
#ifdef IPC_STATS
static int cli_ipc_dump(char *params)
{
    int32_t i = 0;
    struct ipc_ccb *ccb;
    struct ipc_instance *dev;
    struct dl_list *hdr[2];

    dev    = ipc_get_ep_dump();
    hdr[0] = &dev->local_ccb;
    hdr[1] = &dev->remote_ccb;
    CLI_LOG("IRQ\n\trx_irq_cnt %u\n", dev->rx_irq_cnt);
    CLI_LOG("MSG chain\n");
    ipc_msg_print();
    while (i < 2)
    {
        dl_list_for_each(ccb, hdr[i], struct ipc_ccb, list)
        {
            CLI_LOG("%s: %p chan 0x%08x flags 0x%08x \n", ccb->name, ccb, ccb->chan, ccb->flags);
            if (ccb->vq)
            {
                CLI_LOG("\tsize %u num %u\n", ccb->stats.q_size, ccb->stats.q_num);
                if (ccb->flags & IPC_CHAN_FLAGS_REMOTE)
                {
                    CLI_LOG("\ttx_ok %u tx_retry %u tx_failed %u send_notify %u\n",
                        ccb->stats.tx.tx_ok, ccb->stats.tx.tx_retry, ccb->stats.tx.tx_failed, ccb->stats.tx.send_notify);
                    CLI_LOG("\tavail_wr_idx %u avail_rd_idx %u ready_wr_idx %u ready_rd_idx_%u\n", ccb->stats.tx.txq_avail_wr_idx,
                        ccb->stats.tx.txq_avail_rd_idx, ccb->stats.tx.txq_ready_wr_idx , ccb->stats.tx.txq_ready_rd_idx);
                }
                else
                {
                    CLI_LOG("\trx_ok %u rx_retry %u\n",  ccb->stats.rx.rx_ok, ccb->stats.rx.rx_retry);
                    CLI_LOG("\tavail_wr_idx %u avail_rd_idx %u ready_wr_idx %u ready_rd_idx %u\n", ccb->stats.rx.rxq_avail_wr_idx,
                        ccb->stats.rx.rxq_avail_rd_idx, ccb->stats.rx.rxq_ready_wr_idx , ccb->stats.rx.rxq_ready_rd_idx);
                }
            }
        }
        i++;
    }

    return 0;
}
#endif
#ifdef IPC_TEST_CASE
int cli_ipc_test(char *params)
{
    char *token, *next = params;
    struct ipc_test_param param = {0};

    token = utils_next_token(&next);
    if (token == NULL)
        return CLI_SHOW_USAGE;
    param.task_num = 1;
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
                    case 'n':
                        token = utils_next_token(&next);
                        if (!token)
                            return CLI_SHOW_USAGE;
                        param.task_num = atoi(token);
                        if (param.task_num > 4)
                            param.task_num = 4;
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
    param.task_num = 1;
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
                    case 'n':
                        token = utils_next_token(&next);
                        if (!token)
                            return CLI_SHOW_USAGE;
                        param.task_num = atoi(token);
                        if (param.task_num > 4)
                            param.task_num = 4;
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
#ifdef CFG_IPC_PRINT
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

    for (; cli_main_commands[i].exec != NULL; i++)
    {
        CLI_LOG(" - %s %s\r\n", cli_main_commands[i].name, cli_main_commands[i].params);
    }
    /* wifi cli cmd */

#if defined(CLI_TYPE_WF)
    wifi_cmd_handler(buf, 6);
#endif

#if defined(CLI_TYPE_BT)
    bt_cmd_handler("bt?", 4);
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

static int cli_clear_otp(char *params)
{
#if CONFIG_ARCS_HAL_IPC_MRPC_CLIENT_OTP
    nv_selfcali_erase_otp();
#endif

    return CLI_SUCCESS;
}

static int cli_check_otp(char *params)
{
    int32_t ret = 0;
    uint32_t calc_crc = 0;
    ls_nv_fixzone_header_t *hdr = &otp_config.hdr;
    ls_nv_selfcali_body_t *body = &otp_config.bdy;
#if CFG_FLASH_IF
    if (!flash_if_check_security_support()) {
        CLOGE("Flash unsupport OTP region!\n");
        goto failed;
    }
    ret = flash_if_security_read(0, &otp_config, sizeof(otp_config));
    if (ret) {
        CLOGW("Read otp config failed, ret=%d\n", ret);
        goto failed;
    } else {
        CLOGI("Read otp config success\n");
    }
    CLOGI("Otp check magic %x len %x version %x crc32 %x\n", hdr->magic, hdr->length, hdr->version, hdr->crc32);
    if (hdr->magic != NV_MAGIC_PATTERN2) {
        CLOGI("NV fix zone magic (%x) mismatch, skip it!\n", hdr->magic);
        goto failed;
    }
    calc_crc = crc32_sw(calc_crc, (uint8_t *)(hdr), (sizeof(*hdr) - 4));
    calc_crc = crc32_sw(calc_crc, (uint8_t *)(hdr + 1), hdr->length);
    if (calc_crc != hdr->crc32) {
        CLOGI("NV fix zone crc (%x) check failed, expect (%x) skip it!\n", hdr->crc32, calc_crc);
        goto failed;
    }
    CLOGI("Check otp success\n");
    return CLI_SUCCESS;
failed:
    CLOGE("Check otp failed\n");
    return CLI_ERROR;
#else
    CLOGE("unsupport flash_if module!\n");
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

static const struct cli_cmd cli_main_commands[] =
{
#ifndef WIFI_RAM_ATE
    {cli_help, "help", ""},
    {cli_help, "?", ""},
    {net_cli_reboot, "reset", ""},
#if CFG_PING
    {net_cli_ping, "ping",
     "[-s <pkt_size>] [-r <rate>] [-d (duration)] [-Q <ToS>] [-G (background)] <dest_ip>\r\n"
     "     stop <id> [-t (continuous)]"},
#endif
#if CFG_IPERF
    {net_cli_iperf, "iperf", "-s|-c <host>|-h [options (use -h for details)]"},
#endif
    {net_cli_sigkill, "sigkill", "<cmd_id>"},

#if CFG_WIFI_MFG
    {wifi_cli_mfg, "mfg", "config|set|start|stop "
        "[-c <freq>] [-m <mcs idx>] [-s <b|g|n|ax>] [-l <payload length>] [-h] [options (use -h for details)]" },
#endif
    {cli_version,     "version", "show version information"},
#if CFG_MEMDUMP
    {cli_memdump,   "memdump",            "dump memory through uart"},
#endif
#ifndef CFG_AMP_IPC
    {cli_get_chip_temp,    "temp",       "get chip temperature"},
    {cli_temp_por_update,    "temp_por",       "temp por update for testing"},
    {cli_write_efuse,     "efuse_write",     "<addr in word (64-127) (decimal)> <value in hex>"},
    {cli_read_efuse,     "efuse_read",      "<addr in word (0-127) (decimal)>"},
#else
#ifdef IPC_STATS
    {cli_ipc_dump, "ipc_dump", ""},
#endif
#ifdef IPC_TEST_CASE
     {cli_ipc_test, "ipc_test", "[-n number] [-c count]\r\n    stop"},
     {cli_ipc_slave_test, "ipc_slave_test", "[-n number] [-c count]\r\n    stop"},
#endif
#ifdef CFG_IPC_PRINT
     {cli_ipc_dbg, "ipc_dbg", "[on|off]\r\n"},
#endif
#endif
    {cli_mem,     "mem", "[-b] [-w] [-l] r/w addr [len/value]"},
    {cli_log_enable,     "log", "<value> 0: off, 1: on"},
    {cli_log_level_set,     "log_level", "<level> 0~5 : NONE/ERR/WARN/INFO/DEBUG/VERBOSE"},
    {cli_rtos_info,     "rtos_info", "show rtos mem usage and task info"},
    {cli_clear_otp,     "clear_otp", "clear the otp region"},
    {cli_check_otp,     "check_otp", "check the otp region valid or not"},
#if defined(CLI_TYPE_WF)
    {cli_cali_redo,  "redo_cali", "redo rf ppa cap + dpd calibration, should in wifi disconnect or soft ap stop state"},
    {cli_dpd_redo,  "redo_dpd", "redo rf dpd calibration, should in wifi disconnect or soft ap stop state"},
#endif /* CLI_TYPE_WF */
    /* could add other cli cmd below */
#else
    {cli_help, "?", ""},
    {cli_version,     "version", "show version information"},
    {cli_mem,     "mem", "[-b] [-w] [-l] r/w addr [len/value]"},
#if CFG_WIFI_MFG
    {wifi_cli_mfg, "mfg", "config|set|start|stop "
        "[-c <freq>] [-m <mcs idx>] [-s <b|g|n|ax>] [-l <payload length>] [-h] [options (use -h for details)]" },
#endif
#endif
    {NULL, "", ""}
};

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
            CLI_LOG("Usage:\r\n%s %s\r\n",
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

extern uint32_t fhost_cli_handler(char* command, int len);
int32_t cli_shell_process(char *command, int32_t len, int32_t (*func)(uint8_t*, int32_t))
{
    uint32_t res = 0;

    if (!memcmp(command, "wifi", 4))
    {
#if defined(CLI_TYPE_WF)
        res = wifi_cmd_handler(command, len);
#endif
    }
#ifndef WIFI_RAM_ATE
#if defined(CLI_TYPE_BT)
    else if (!memcmp(command, "bt", 2) || !memcmp(command, "ble", 3))
    {
        res = bt_cmd_handler(command, len);
    }
#endif
#ifdef CFG_ATCMD
    else if (!memcmp(command, "AT", 2))
    {
        atcmd_handler(command, len);
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
