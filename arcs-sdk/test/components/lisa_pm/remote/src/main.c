/*
 * wifi_pm 独立 AP 固件
 *
 * 启动流程：
 *   1. SDK startup.S 早期钩子 arcs_early_startup_hook() 在 sp/gp/MTVT 设置完成
 *      之后、scatload 之前调用 ap_startup_dispatch()。该阶段 .data 未拷贝、
 *      .bss 未清零，dispatch 只能用寄存器和 MMIO，禁止访问任何全局变量。
 *      - REG_AON_DIG_RSVD0 == JUMP_NONE: 清标志后进入 WFI 死循环，不返回
 *      - REG_AON_DIG_RSVD0 == JUMP_RAM : 把 CP 复位到 RAM 入口后进入 WFI 死循环，
 *                                        不返回
 *      - 其它（冷启动）: dispatch 直接 return，SDK 通用启动继续
 *   2. SDK 跑完 scatload + soc_init + freertos 后进入 main()
 *   3. main() 引导 CP 从 flash 起来，然后 __WFI() 空转
 *
 * 不再使用独立的 boot.S / linker.ld；启动流程完全交给 SDK 通用启动。
 */

#include <stdint.h>

#include "chip.h"
#include "memap.h"

#include "IOMuxManager.h"

#define TAG "wifi_pm_ap"
#include "lisa_log.h"

#define WAKEUP_ACT_JUMP_RAM     (0xAA)
#define WAKEUP_ACT_JUMP_NONE    (0xFF)

void lisa_uart1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_ALTER3);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER3);
}

/*
 * 早期 PM 唤醒分支：在 SDK startup.S 的 arcs_early_startup_hook 里调用。
 * 此函数运行在 .data/.bss 未初始化的极早期，禁止使用任何全局变量。
 * 仅处理 JUMP_NONE / JUMP_RAM；冷启动直接 return，由 main() 走正常 flash 引导。
 */
static void ap_startup_dispatch(void)
{
    uint32_t act = IP_AON_CTRL->REG_AON_DIG_RSVD0.all;

    if (act == WAKEUP_ACT_JUMP_NONE) {
        IP_AON_CTRL->REG_AON_DIG_RSVD0.all = 0;
        goto wfi_loop;
    }

    if (act == WAKEUP_ACT_JUMP_RAM) {
        if ((IP_AON_CTRL->REG_WAKEUP_ISR.all == 0) &&
            (IP_AON_CTRL->REG_AON_DIG_RSVD2.all == WAKEUP_ACT_JUMP_RAM)) {
            IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = IP_AON_CTRL->REG_AON_DIG_RSVD3.all;
            IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
        }
        goto wfi_loop;
    }

    return;

wfi_loop:
    do {
        __WFI();
    } while (1);
}

/* SDK startup.S 的 weak 钩子：在 scatload 之前调用，覆盖默认 no-op 实现。 */
void arcs_early_startup_hook(void)
{
    ap_startup_dispatch();
}

static void boot_cp_from_flash(void)
{
    LOGI("boot cp from flash: 0x%x", (unsigned)MEM_CP_FLASH_BASE);
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    boot_cp_from_flash();

    LOGI("ap idle");

    while (1) {
        __WFI();
    }
    return 0;
}
