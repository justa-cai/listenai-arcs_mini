#include "stdint.h"
#include "stdio.h"
#include "shell.h"
#include "cmd.h"
#include "stddef.h"
#include "string.h"
#include "bt_app_if.h"

static int ble_cmd_help(int argc, char **argv);

/**
 * @brief Start BLE network configuration mode
 *
 * @param argc Argument count
 * @param argv Argument values: [adv_id] [adv_type]
 * @return int 0: success, -1: failed
 */
static int ble_net_start(int argc, char **argv)
{
    uint8_t adv_id = 0;
    uint8_t adv_type = BLE_ADV_GEN;

    if (argc >= 1) {
        adv_id = (uint8_t)atoi(argv[0]);
    }

    if (argc >= 2) {
        adv_type = (uint8_t)atoi(argv[1]);

        if (adv_type > BLE_ADV_DIR_HDC) {
            printf("Invalid adv_type. Valid values: 0-3\n");
            printf("  0: BLE_ADV_GEN (General)\n");
            printf("  1: BLE_ADV_GEN_PAIRED (General Paired)\n");
            printf("  2: BLE_ADV_DIR (Directed)\n");
            printf("  3: BLE_ADV_DIR_HDC (Directed High Duty Cycle)\n");
            return -1;
        }
    }

    printf("Starting BLE advertising (adv_id=%d, adv_type=%d)...\n", adv_id, adv_type);

    uint8_t ret = app_ble_adv_start(adv_id, adv_type);
    if (ret != pdTRUE) {
        printf("Failed to start BLE advertising, error code: %d\n", ret);
        return -1;
    }

    printf("BLE advertising started successfully\n");
    return 0;
}

/**
 * @brief Stop BLE network configuration mode
 *
 * @param argc Argument count
 * @param argv Argument values: [adv_id]
 * @return int 0: success, -1: failed
 */
static int ble_net_stop(int argc, char **argv)
{
    uint8_t adv_id = 0;

    if (argc >= 1) {
        adv_id = (uint8_t)atoi(argv[0]);
    }

    printf("Stopping BLE advertising (adv_id=%d)...\n", adv_id);

    uint8_t ret = app_ble_adv_stop(adv_id);
    if (ret != pdTRUE) {
        printf("Failed to stop BLE advertising, error code: %d\n", ret);
        return -1;
    }

    printf("BLE advertising stopped successfully\n");
    return 0;
}

static const struct listen_cmd_t g_ble_cmds[] = {
    {"start_net_adv", ble_net_start, "start BLE advertising, ex: ble start [adv_id] [adv_type]"},
    {"stop_net_adv", ble_net_stop, "stop BLE advertising, ex: ble stop [adv_id]"},
    {"help", ble_cmd_help, "show help"},
};

/**
 * @brief Show help information
 */
static int ble_cmd_help(int argc, char **argv)
{
    int cmd_len = sizeof(g_ble_cmds) / sizeof(g_ble_cmds[0]);

    printf("BLE Network Configuration Commands:\n");
    printf("-----------------------------------\n");

    for (int i = 0; i < cmd_len; i++) {
        if (g_ble_cmds[i].help != NULL) {
            printf("%-17s\t:\t%s\n", g_ble_cmds[i].name, g_ble_cmds[i].help);
        }
    }

    printf("\nAdv Type Values:\n");
    printf("  0: BLE_ADV_GEN (General)\n");
    printf("  1: BLE_ADV_GEN_PAIRED (General Paired)\n");
    printf("  2: BLE_ADV_DIR (Directed)\n");
    printf("  3: BLE_ADV_DIR_HDC (Directed High Duty Cycle)\n");

    return 0;
}

/**
 * @brief BLE command main handler
 */
static int ble_cmd_handler(int argc, char **argv)
{
    if (argc == 1) {
        ble_cmd_help(argc, argv);
        return 0;
    }

    int i;
    for (i = 0; i < sizeof(g_ble_cmds) / sizeof(g_ble_cmds[0]); i++) {
        if (strcmp(g_ble_cmds[i].name, argv[1]) == 0) {
            return g_ble_cmds[i].exec(argc - 2, argv + 2);
        }
    }

    printf("Unknown command: %s\n", argv[1]);
    ble_cmd_help(argc, argv);

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ble,
                 ble_cmd_handler, BLE network configuration commands);
