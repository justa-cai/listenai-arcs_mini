#include "shell.h"
#include <stdlib.h>
#include "arcs_ap.h"

static int shell_mic_bias_voltage(int argc, char **argv)
{
    Shell* shell = shellGetCurrent();

    if (argc != 2) {
        shellPrint(shell, "Usage: mic_bias_voltage <voltage>\r\n");
        shellPrint(shell, "  Available voltage levels:\r\n");
        shellPrint(shell, "    1.68   -> 1.68V  (TUNE_LDOVA = 0x8)\r\n");
        shellPrint(shell, "    1.785  -> 1.785V (TUNE_LDOVA = 0xf)\r\n");
        shellPrint(shell, "    1.8    -> 1.8V   (TUNE_LDOVA = 0x0)\r\n");
        shellPrint(shell, "    1.815  -> 1.815V (TUNE_LDOVA = 0x1)\r\n");
        shellPrint(shell, "    1.905  -> 1.905V (TUNE_LDOVA = 0x7) [default]\r\n");
        shellPrint(shell, "  Or use register value directly (0x0-0xf)\r\n");
        shellPrint(shell, "  Example: mic_bias_voltage 1.905\r\n");
        shellPrint(shell, "  Example: mic_bias_voltage 0x7\r\n");
        return -1;
    }

    uint8_t tune_value;
    char *voltage_str = argv[1];

    /* 检查是否是16进制格式的寄存器值 */
    if (voltage_str[0] == '0' && (voltage_str[1] == 'x' || voltage_str[1] == 'X')) {
        /* 直接使用寄存器值 */
        tune_value = (uint8_t)strtol(voltage_str, NULL, 16);

        /* 检查范围: 0x0-0xf (4-bit) */
        if (tune_value > 0xf) {
            shellPrint(shell, "Error: register value out of range (0x0 to 0xf)\r\n");
            return -1;
        }
    } else {
        /* 根据电压值转换为寄存器值 */
        if (strcmp(voltage_str, "1.68") == 0) {
            tune_value = 0x8;
        } else if (strcmp(voltage_str, "1.785") == 0) {
            tune_value = 0xf;
        } else if (strcmp(voltage_str, "1.8") == 0) {
            tune_value = 0x0;
        } else if (strcmp(voltage_str, "1.815") == 0) {
            tune_value = 0x1;
        } else if (strcmp(voltage_str, "1.905") == 0) {
            tune_value = 0x7;
        } else {
            shellPrint(shell, "Error: unsupported voltage level '%s'\r\n", voltage_str);
            shellPrint(shell, "Supported values: 1.68, 1.785, 1.8, 1.815, 1.905 or 0x0-0xf\r\n");
            return -1;
        }
    }

    /* 配置 MIC BIAS 电压 */
    IP_AON_CTRL->REG_AON_TUNE1.bit.TUNE_LDOVA = tune_value;

    shellPrint(shell, "MIC BIAS voltage updated: TUNE_LDOVA = 0x%x\r\n", tune_value);

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, mic_bias_voltage,
                 shell_mic_bias_voltage, "set mic bias voltage <voltage>");