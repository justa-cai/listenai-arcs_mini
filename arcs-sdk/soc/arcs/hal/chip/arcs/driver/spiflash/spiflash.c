#include <string.h>
#include "platform.h"
#include "spiflash.h"
#include "arcs_ap.h"
#include "cache.h"

#pragma GCC optimize ("-fno-jump-tables")

#ifdef CFG_RTOS
extern void vPortEnterCritical(void);
extern void vPortExitCritical(void);
#endif

//define it for not interrupt by isr or other task
#define SPIROM_NO_INTERRUPT

#define MemBarrier()          __COMPILER_BARRIER()

#undef printf
#define printf(format, ...)    ((void)0)

#ifdef CFG_RTOS
#define FLASH_DRIVER_PROTECT()          vPortEnterCritical()
#define FLASH_DRIVER_UNPROTECT()        vPortExitCritical()
#else
#define FLASH_DRIVER_PROTECT()          disable_GINT()
#define FLASH_DRIVER_UNPROTECT()        enable_GINT()
#endif

#ifdef CONFIG_DUAL_FLASH
_EXT_RAM void flash_dualflash_config(uint32_t flash0_low, uint32_t flash0_high)
{
    // Range of Flash0: [flash0_low - flash0_high - 1], aligned to 4K
    // Beyond this range, Flash1 is used
    outw(0x47600054, ((flash0_high >> 12) << 16) | flash0_low);
}

__STATIC_FORCEINLINE void flash_dualflash_enable_excl(uint32_t flash_id)
{
    if(flash_id == 1)
    {
        //Force disable Flash0, enable Flash1
        outw(0x47600058, 0xffff0006);
    }
    else
    {
        //Force disable Flash1, enable Flash0
        outw(0x47600058, 0xffff0009);
    }
}

__STATIC_FORCEINLINE void flash_dualflash_enable_both(void)
{
    // Dual Flash enabled
    outw(0x47600058, 0xffff0000);
}

__STATIC_FORCEINLINE int flash_dualflash_id_get(uint32_t offset)
{
    // Get Flash ID
    offset >>= 12;
    uint32_t config = inw(0x47600054);
    uint32_t config_low = config & 0xFFFF;
    uint32_t config_high = (config >> 16) & 0xFFFF;

    if(offset >= config_low && offset < config_high) {
        return 0; // Flash0
    } else {
        return 1; // Flash1
    }

}
#endif // CONFIG_DUAL_FLASH


extern int mxic_check(FLASH_DEV *dev);
extern int mxic_program(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize);
extern int mxic_erase(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize);
extern int mxic_erase_page(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize);
extern int mxic_read(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize);
extern int mxic_lock(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize);
extern int mxic_unlock(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize);
extern int mxic_set_wrsr(FLASH_DEV *dev, unsigned int uiStat);
extern int mxic_enable_qd(FLASH_DEV *dev);
extern int mxic_security_program(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize);
extern int mxic_security_erase(FLASH_DEV *dev, unsigned int FlashAddr);
extern int mxic_security_read(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize);
extern int mxic_deep_power_down(FLASH_DEV *dev);
extern int mxic_release_deep_power_down(FLASH_DEV *dev);
extern int mxic_write_lock_bit(FLASH_DEV *dev, unsigned char value);

static int mxic_wr_en(FLASH_DEV *dev);
static int mxic_wr_rdy(FLASH_DEV *dev);
static int mxic_id_rems(FLASH_DEV *dev, unsigned short *id_rems);

// udx: user defined
_EXT_RAM int flash_init(FLASH_DEV *dev, unsigned char ud0, unsigned char ud1)
{
    int ret;

    dev->w_protect = true;
    if(dev->timeout == 0) {
        dev->timeout = FLASH_RETRY_TIMES;
    }
    if(RUN_WITHOUT_INT == dev->run_mod) {
        FLASH_DRIVER_PROTECT();
    }

#ifdef CONFIG_DUAL_FLASH
    flash_dualflash_enable_excl(0);
#endif

    ret = platform_init(dev, ud0, ud1);

#ifdef CONFIG_DUAL_FLASH
    flash_dualflash_enable_excl(1);
    ret |= platform_init(dev, ud0, ud1);
    flash_dualflash_enable_both();
#endif

    if(RUN_WITHOUT_INT == dev->run_mod) {
        FLASH_DRIVER_UNPROTECT();
    }

    return ret;
}

_EXT_RAM int flash_read(FLASH_DEV *dev, off_t offset, void *data, size_t len)
{
    int ret = -1;

    do {
        if(dev == NULL) break;
        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_PROTECT();
        }
        ret = mxic_read(dev, (unsigned int)offset, (unsigned char *)data, (unsigned int)len);
        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_UNPROTECT();
        }
    } while(0);

    return ret;
}

_EXT_RAM int flash_write(FLASH_DEV *dev, off_t offset, const void *data, size_t len)
{
    int ret = -1;
    volatile int result;

    if(dev && dev->w_protect == false) {
        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_PROTECT();
        }

#ifdef CONFIG_DUAL_FLASH
        flash_dualflash_enable_excl(flash_dualflash_id_get(offset));
#endif

        ret = mxic_program(dev, (unsigned int)offset, (unsigned char *)data, (unsigned int)len);

#ifdef CONFIG_DUAL_FLASH
        flash_dualflash_enable_both();
#endif

        if((IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_TGT_SLV_SEL == 1) && (IP_SYSCTRL->REG_CIPHER_CTRL3.bit.CIPHER_EN_REGION_A == 1)) {
            __RV_CSR_WRITE(CSR_CCM_MCOMMAND, CCM_DC_INVAL_ALL);
            __RWMB();
            __FENCE_I();

            result = *((int *)CP_CIPHER_REGION_A);
            MemBarrier();
        }

        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_UNPROTECT();
        }
    }

    return ret;
}

_EXT_RAM int flash_erase(FLASH_DEV *dev, off_t offset, size_t size)
{
    int ret = -1;

    if(dev && dev->w_protect == false) {
        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_PROTECT();
        }

#ifdef CONFIG_DUAL_FLASH
        flash_dualflash_enable_excl(flash_dualflash_id_get(offset));
#endif

        ret = mxic_erase(dev, (unsigned int)offset, (unsigned int)size);

#ifdef CONFIG_DUAL_FLASH
        flash_dualflash_enable_both();
#endif

        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_UNPROTECT();
        }
    }

    return ret;
}

_EXT_RAM int flash_erase_page(FLASH_DEV *dev, off_t offset, size_t size)
{
    int ret = -1;

    if(dev && dev->w_protect == false) {
        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_PROTECT();
        }

#ifdef CONFIG_DUAL_FLASH
        flash_dualflash_enable_excl(flash_dualflash_id_get(offset));
#endif

        ret = mxic_erase_page(dev, (unsigned int)offset, (unsigned int)size);

#ifdef CONFIG_DUAL_FLASH
        flash_dualflash_enable_both();
#endif 

        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_UNPROTECT();
        }
    }

    return ret;
}

_EXT_RAM int flash_security_read(FLASH_DEV *dev, off_t offset, void *data, size_t len)
{
    int ret = -1;

    do {
        if(dev == NULL) break;

        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_PROTECT();
        }

        ret = mxic_security_read(dev, (unsigned int)offset, (unsigned char *)data, (unsigned int)len);

        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_UNPROTECT();
        }

    } while(0);

    return ret;
}

_EXT_RAM int flash_security_write(FLASH_DEV *dev, off_t offset, const void *data, size_t len)
{
    int ret = -1;

    if(dev && dev->w_protect == false) {

        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_PROTECT();
        }

        ret = mxic_security_program(dev, (unsigned int)offset, (unsigned char *)data, (unsigned int)len);

        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_UNPROTECT();
        }

    }

    return ret;
}

_EXT_RAM int flash_security_erase(FLASH_DEV *dev, off_t offset)
{
    int ret = -1;

    if(dev && dev->w_protect == false) {
        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_PROTECT();
        }

        ret = mxic_security_erase(dev, (unsigned int)offset);

        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_UNPROTECT();
        }
    }

    return ret;
}

_EXT_RAM int flash_status_register_get(FLASH_DEV *dev, unsigned char reg_addr, uint32_t *data_out)
{
    int ret = -1;

    unsigned int cmd_rd, cmd_wr;
    int result = 0;

    if(reg_addr == 1) {
        cmd_rd = SPIROM_CMD_RDST;
        cmd_wr = SPIROM_CMD_WRSR;
    } else if (reg_addr == 2) {
        cmd_rd = SPIROM_CMD_RDST2;
        cmd_wr = SPIROM_CMD_WRSR2;
    } else if (reg_addr == 3) {
        cmd_rd = SPIROM_CMD_RDST3;
        cmd_wr = SPIROM_CMD_WRSR3;
    } else {
        return ret;
    }

    if(RUN_WITHOUT_INT == dev->run_mod) {
        FLASH_DRIVER_PROTECT();
    }

    do {
        result = spirom_cmd_send(dev, cmd_rd, 0x0, 0, NULL, (unsigned int*)data_out);
        if(result) break;

    } while(0);

    ret = result;

    if(RUN_WITHOUT_INT == dev->run_mod) {
        FLASH_DRIVER_UNPROTECT();
    }

    return ret;
}

_EXT_RAM int flash_status_register_set(FLASH_DEV *dev, unsigned char reg_addr, const uint32_t data_in, uint32_t *data_out)
{
    int ret = -1;

    if(dev && dev->w_protect == false) {

        unsigned int cmd_rd, cmd_wr;
        unsigned int buf_in, buf_out;
        int result = 0;

        if(reg_addr == 1) {
            cmd_rd = SPIROM_CMD_RDST;
            cmd_wr = SPIROM_CMD_WRSR;
        } else if (reg_addr == 2) {
            cmd_rd = SPIROM_CMD_RDST2;
            cmd_wr = SPIROM_CMD_WRSR2;
        } else if (reg_addr == 3) {
            cmd_rd = SPIROM_CMD_RDST3;
            cmd_wr = SPIROM_CMD_WRSR3;
        } else {
            return ret;
        }

        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_PROTECT();
        }

        do {
            result = mxic_wr_en(dev);
            if(result) break;
            result = spirom_cmd_send(dev, cmd_wr, data_in, 0, NULL, &buf_out);
            if(result) break;
            /*-- get enable status --*/
            result = mxic_wr_rdy(dev);
            if(result) break;

            result = spirom_cmd_send(dev, cmd_rd, 0x0, 0, NULL, &buf_out);
            if(result) break;

            *data_out = buf_out;
        } while(0);

        ret = result;

        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_UNPROTECT();
        }
    }

    return ret;
}

_EXT_RAM int flash_write_protection_set(FLASH_DEV *dev, bool enable)
{
    int ret = -1;

    if(dev) {
        ((FLASH_DEV *)dev)->w_protect = enable;
        ret = 0;
    }
    return ret;
}

_EXT_RAM size_t flash_get_write_block_size(FLASH_DEV *dev)
{
    return -1;
}

_EXT_RAM int flash_get_page_info_by_offs(FLASH_DEV *dev, off_t offset, struct flash_pages_info *info)
{
    info->index = offset / SPIROM_SECTOR_SIZE;
    info->size = SPIROM_SECTOR_SIZE;
    info->start_offset = offset & (~(SPIROM_SECTOR_SIZE - 1));
    return 0;
}

_EXT_RAM int flash_id(FLASH_DEV *dev, uint32_t *id_manufacturer, uint32_t *id_device)
{
    int ret = -1;
    unsigned short id_rems = 0;

    if(dev) {
        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_PROTECT();
        }

        ret = mxic_id_rems(dev, &id_rems);;
        if(ret == 0) {
            if(id_manufacturer != NULL) {
                *id_manufacturer = (id_rems) & 0xff; // Manufacturer ID
            }
            if(id_device != NULL) {
                *id_device = (id_rems >> 8) & 0xff; // Device ID
            };
        }

        if(RUN_WITHOUT_INT == dev->run_mod) {
            FLASH_DRIVER_UNPROTECT();
        }
    }

    return ret;
}

_EXT_RAM unsigned int spirom_prepare_cmd(unsigned int cmd, unsigned int addr)
{
    unsigned int b0 = (cmd & 0xff);
    unsigned int b1 = (((addr >> 16) & 0xff) << 8);
    unsigned int b2 = (((addr >> 8) & 0xff) << 16);
    unsigned int b3 = ((addr & 0xff) << 24);
    unsigned int word = (b0 | b1 | b2 | b3);
    return word;
}

_EXT_RAM int
spirom_cmd_send(FLASH_DEV *dev, unsigned int cmd, unsigned int addr, unsigned int bytes, unsigned int* pdata, unsigned int* Retdata)
{
    unsigned int spib_dctrl;
    unsigned int op_addr, op;
    unsigned int data = 0;
    unsigned int spib_busy = 0;
    unsigned long spib_rx_empty = 0, base = dev->base_addr;
    /*-- wait if there is active transaction --*/
    spib_busy = spib_wait_spi(base);
    if(spib_busy != 0) {
        return -1;
    }
    /*-- clear tx/rx fifo --*/
    spib_clr_fifo(base);

    /*-- prepare opcode and address --*/
    switch(cmd) {
    case SPIROM_CMD_READ:
        if(dev->d_width == 4) {
            spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_DY_RD, 0, 1, bytes - 1, 1, 2, 1);
            op = dev->addr_bytes == 4 ? SPIROM_OP_QFAST_READA4 : SPIROM_OP_QFAST_READ;
        } else if(dev->d_width == 2) {
            spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_DY_RD, 0, 0, bytes - 1, 1, 1, 0);
            op = dev->addr_bytes == 4 ? SPIROM_OP_DFAST_READA4 : SPIROM_OP_DFAST_READ;
        } else {
            spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_DY_RD, 0, 0, bytes - 1, 0, 0, 0);
            op = dev->addr_bytes == 4 ? SPIROM_OP_FAST_READA4 : SPIROM_OP_FAST_READ;
        }
        spib_exe_cmmd2(base, op, addr, spib_dctrl);
        spib_rx_data(base, pdata, bytes);
        break;
    case SPIROM_CMD_WREN:
        op_addr = SPIROM_OP_WREN;
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        break;
    case SPIROM_CMD_WRDI:
        op_addr = SPIROM_OP_WRDI;
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        break;
    case SPIROM_CMD_ERASE:
        op = dev->addr_bytes == 4 ? SPIROM_OP_SEA4 : SPIROM_OP_SE;
        spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_NONE, 0, 0, 0, 0, 0, 0);
        spib_exe_cmmd2(base, op, addr, spib_dctrl);
        break;
    case SPIROM_CMD_ERASE_B32:
        op = dev->addr_bytes == 4 ? SPIROM_OP_BEA4 : SPIROM_OP_BE;
        spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_NONE, 0, 0, 0, 0, 0, 0);
        spib_exe_cmmd2(base, op, addr, spib_dctrl);
        break;
    case SPIROM_CMD_ERASE_B64:
        op = dev->addr_bytes == 4 ? SPIROM_OP_BE2A4 : SPIROM_OP_BE2;
        spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_NONE, 0, 0, 0, 0, 0, 0);
        spib_exe_cmmd2(base, op, addr, spib_dctrl);
        break;
    case SPIROM_CMD_ERASE_PG:
        op_addr = spirom_prepare_cmd(SPIROM_OP_PE, addr);
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 3, 0, 0);
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        break;
    case SPIROM_CMD_PROGRAM:
        if(dev->d_width == 4) {
            op = dev->addr_bytes == 4 ? SPIROM_OP_QPPA4 : SPIROM_OP_QPP;
            spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_WRonly, bytes - 1, 0, 0, 0, 2, 0);
        } else {
            op = dev->addr_bytes == 4 ? SPIROM_OP_PPA4 : SPIROM_OP_PP;
            spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_WRonly, bytes - 1, 0, 0, 0, 0, 0);
        }
        spib_exe_cmmd2(base, op, addr, spib_dctrl);
        spib_tx_data(base, pdata, bytes);
        break;
    case SPIROM_CMD_RDID:
        op_addr = SPIROM_OP_RDID;
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WR_RD, 0, 0, 2);
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        spib_rx_empty = spib_wait_rx_empty(base);
        if(spib_rx_empty == 0) {
            data = spib_get_data(base);
        }
        break;
    case SPIROM_CMD_RDST:
    case SPIROM_CMD_RDST2:
    case SPIROM_CMD_RDST3:
        if(cmd == SPIROM_CMD_RDST) {
            op_addr = SPIROM_OP_RDSR;
        }
        else if(cmd == SPIROM_CMD_RDST2) {
            op_addr = SPIROM_OP_RDSR2;
        }
        else {
            op_addr = SPIROM_OP_RDSR3;
        }
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WR_RD, 0, 0, 0);
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        spib_rx_empty = spib_wait_rx_empty(base);
        if(spib_rx_empty == 0) {
            data = spib_get_data(base);
        }
        break;
    case SPIROM_CMD_LOCK:
        op_addr = spirom_prepare_cmd(SPIROM_OP_SBLK, addr);
        // spib_dctrl = spib_prepare_dctrl (0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 3, 0, 0);
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        break;
    case SPIROM_CMD_UNLOCK:
        op_addr = spirom_prepare_cmd(SPIROM_OP_SBULK, addr);
        // spib_dctrl = spib_prepare_dctrl (0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 3, 0, 0);
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        break;
    case SPIROM_CMD_RDBLOCK:
        op_addr = spirom_prepare_cmd(SPIROM_OP_RDBLOCK, addr);
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WR_RD, 3, 0, 0);
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        spib_rx_empty = spib_wait_rx_empty(base);
        if(spib_rx_empty == 0) {
            data = spib_get_data(base);
        }
        break;
    case SPIROM_CMD_RDSCUR:
        op_addr = SPIROM_OP_RDSCUR;
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WR_RD, 0, 0, 0);
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        spib_rx_empty = spib_wait_rx_empty(base);
        if(spib_rx_empty == 0) {
            data = spib_get_data(base);
        }
        break;
    case SPIROM_CMD_CHIP_UNLOCK:
        op_addr = SPIROM_OP_GBULK;
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        break;
    case SPIROM_CMD_WRSR:
    case SPIROM_CMD_WRSR2:
    case SPIROM_CMD_WRSR3:
        if(cmd == SPIROM_CMD_WRSR) {
            op_addr = (SPIROM_OP_WRSR | (addr << 8));
        }
        else if(cmd == SPIROM_CMD_WRSR2) {
            op_addr = (SPIROM_OP_WRSR2 | (addr << 8));
        }
        else {
            op_addr = (SPIROM_OP_WRSR3 | (addr << 8));
        }
//        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 1, 0, 0);
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, bytes + 1, 0, 0);
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        break;
    case SPIROM_CMD_RESET_DEV:
        spib_dctrl = spib_prepare_dctrl(0x1, 0x0, SPIB_TM_NONE, 0, 0, 0);
        op_addr = SPIROM_OP_EN_RESET;
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        op_addr = SPIROM_OP_RESET;
        spib_exe_cmmd(base, op_addr, spib_dctrl);
        break;
    case SPIROM_CMD_READ_SFDP:
        op_addr = SPIROM_OP_RDSFDP;

        spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_DY_RD, 0, 0, bytes - 1, 0, 0, 0);
        spib_exe_cmmd2(base, op_addr, addr, spib_dctrl);
        spib_rx_data(base, pdata, bytes);
        break;
    case SPIROM_CMD_RUID:
        op_addr = SPIROM_OP_RUID;
        spib_dctrl = spib_prepare_dctrl2(0x1, 0, SPIB_TM_DY_RD, 0, 3, bytes - 1, 0 ,0, 0); //4 dummy cycle
        spib_exe_cmmd2(base, op_addr, addr, spib_dctrl);
        spib_rx_data(base, pdata, bytes);
        break;
    case SPIROM_CMD_SEC_PRGM:
        op = SPIROM_OP_SEC_PRGM;
        spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_WRonly, bytes - 1, 0, 0, 0, 0, 0);
        spib_exe_cmmd2(base, op, addr, spib_dctrl);
        spib_tx_data(base, pdata, bytes);
        break;
    case SPIROM_CMD_SEC_ERASE:
        op = SPIROM_OP_SEC_ERASE;
        spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_NONE, 0, 0, 0, 0, 0, 0);
        spib_exe_cmmd2(base, op, addr, spib_dctrl);
        break;
    case SPIROM_CMD_SEC_READ:
        op = SPIROM_OP_SEC_READ;
        spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_DY_RD, 0, 0, bytes - 1, 0, 0, 0);
        spib_exe_cmmd2(base, op, addr, spib_dctrl);
        spib_rx_data(base, pdata, bytes);
        break;
    case SPIROM_CMD_PD:
        op = SPIROM_OP_POW_DOWN;
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
        spib_exe_cmmd(base, op, spib_dctrl);
        break;
    case SPIROM_CMD_REL_PD:
        op = SPIROM_OP_REL_POW_DOWN;
        spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
        spib_exe_cmmd(base, op, spib_dctrl);
        break;
    case SPIROM_CMD_REMSID:
        op = SPIROM_OP_READ_ID;
        spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_RDonly, 0, 0, bytes - 1, 0, 0, 0);
        spib_exe_cmmd2(base, op, addr, spib_dctrl);
        spib_rx_data(base, pdata, bytes);
        break;
    default:
        printf("spirom_cmd_send: wrong cmd\n");
        return -1;
        break;
    }
    spib_busy = spib_wait_spi(base);
    //errors
    if(spib_busy || spib_rx_empty) {
        return -1;
    }
    *Retdata = data;
    return 0;
}

_EXT_RAM static int mxic_wr_en(FLASH_DEV *dev)
{
    unsigned int result, RetData, j;

    result = spirom_cmd_send(dev, SPIROM_CMD_WREN, 0x0, 0, NULL, &RetData);
    if(result != 0) {
        printf("mxic_program: (program) page %d enable write fail\n", i);
        return -1;
    }
    for(j = 1; j < dev->timeout; j++) {
        /*-- get enable status --*/
        result = spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &RetData);
        if(result != 0) {
            printf("mxic_program: get (program) page %d write enable status fail\n", i);
            return -1;
        }
        if(RetData & SPIROM_SR_WEL_MASK)
            break;
    }
    if((RetData & SPIROM_SR_WEL_MASK) == 0) {
        printf("mxic_program: (program) page %d, write enable is not set (status %x)\n", i, RetData);
        return -1;
    }
    return 0;
}

_EXT_RAM static int mxic_wr_rdy(FLASH_DEV *dev)
{
    unsigned int result, RetData = 0, j;

    for(j = 1; j < dev->timeout; j++) {
        result = spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &RetData);
        if(result != 0) {
            printf("mxic_program: get (program) page %d write enable status fail\n", i);
            return -1;
        }
        if((RetData & (SPIROM_SR_WIP_MASK | SPIROM_SR_WEL_MASK)) == 0)
            break;
    }
    if((RetData & (SPIROM_SR_WIP_MASK | SPIROM_SR_WEL_MASK)) != 0) {
        printf("mxic_program: (program) page %d, write enable is not clear (status %x)\n", i, RetData);
        return -1;
    }
    return 0;
}


_EXT_RAM int spirom_set_extaddr(FLASH_DEV *dev, unsigned int addr)
{
#define EXT_ADDR_W     0xc5
#define EXT_ADDR_R     0xc8
    int result = 0;
    unsigned int op_addr, spib_dctrl, data;
    unsigned long base = dev->base_addr;

    data = 0;
    // read extend addr
    spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WR_RD, 0, 0, 0);
    spib_exe_cmmd(base, EXT_ADDR_R, spib_dctrl);
    result = spib_wait_rx_empty(base);
    if(result == 0) {
        data = spib_get_data(base);
    }

    // bit24 of address is different, write it
    if((addr >> 24) != data) {
        do {
            result = mxic_wr_en(dev);
            if(result) break;

            addr = (addr >> 16) & 0xff00;
            op_addr = EXT_ADDR_W | addr;
            spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 1, 0, 0);
            spib_exe_cmmd(base, op_addr, spib_dctrl);

            result = spib_wait_spi(base);
        } while(0);
    }

    return result;
}

_EXT_RAM int spirom_set_addr4(FLASH_DEV *dev, int en)
{
#define EXT_SET_ADDR4     0xb7
#define EXT_CLR_ADDR4     0xe9
    unsigned int op_addr, spib_dctrl;
    unsigned long base = dev->base_addr;

    if(en) op_addr = EXT_SET_ADDR4;
    else   op_addr = EXT_CLR_ADDR4;
    spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
    spib_exe_cmmd(base, op_addr, spib_dctrl);

    return spib_wait_spi(base);
}

_EXT_RAM int mxic_unlock_chip(FLASH_DEV *dev)
{
    unsigned int RetData;
    int result = 0;
    do {
        /*-- write enable --*/
        result = mxic_wr_en(dev);
        if(result) break;
        /*-- Chip-UnLock --*/
        result = spirom_cmd_send(dev, SPIROM_CMD_CHIP_UNLOCK, 0x0, 0, NULL, &RetData);
        if(result) break;
        /*-- get ULock status --*/
        result = mxic_wr_rdy(dev);
        if(result) break;
        result = spirom_cmd_send(dev, SPIROM_CMD_RDBLOCK, 0, 0, NULL, &RetData);
        if(result) break;

        if((RetData & 0xFF) == 0xFF) {
            printf("mxic_unlock_chip: ULock-chip fail\n");
            result = -1;
        }
    } while(0);

    return result;
}
// TODO: modify this function to target-specific function - check flash

//unsigned int FlashType;
_EXT_RAM int mxic_check(FLASH_DEV *dev)
{
    unsigned long result, RetData, FlashId, i, base = dev->base_addr;
    unsigned char temp[4];
    unsigned int SCLK_DIV = 0xff;    //SCLK is the same as the SPI clock source

    RetData = (spib_get_regtiming(base) & (~0xFF));
    spib_set_regtiming(base, RetData | SCLK_DIV);
    SCLK_DIV = spib_get_regtiming(base);
    printf("mxic_check: SCLK_DIV=%x\n", SCLK_DIV);

    do {
        result = spirom_cmd_send(dev, SPIROM_CMD_RDID, 0x0, 0, NULL, (unsigned int*)temp);
        if(result != 0) {
            printf("ERROR: read spi rom id fail\n");
            break;
        }

        FlashId = temp[0] << 16 | temp[1] << 8 | temp[2];

        printf("Flash type is 0x%08x\n", FlashId);
    } while(0);

    return result;
}
// TODO: modify this function to target-specific function - erase flash
_EXT_RAM int mxic_erase(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize)
{
    unsigned int EraseAddrStart, EraseSize, EraseSectorCnt, /*EraseSectorIndex,*/ i, j;
    unsigned int result, RetData, timeout = dev->timeout;

    EraseAddrStart = (FlashAddr / SPIROM_SECTOR_SIZE) * SPIROM_SECTOR_SIZE;
    EraseSize = (FlashAddr - EraseAddrStart) + DataSize;
    EraseSectorCnt = (EraseSize + (SPIROM_SECTOR_SIZE - 1)) / SPIROM_SECTOR_SIZE;
//    EraseSectorIndex = (EraseAddrStart / SPIROM_SECTOR_SIZE);

    for(i = 0; i < EraseSectorCnt; ) {
        /*---------------------*/
        /*-- ERASE procedure   */
        /*---------------------*/
        /*-- write enable --*/
        result = mxic_wr_en(dev);
        if(result) break;
        /*-- erase --*/
        if(!(EraseAddrStart & SPIROM_BLK64_MASK) && (EraseSectorCnt >= (SPIROM_BLK64_SIZE / SPIROM_SECTOR_SIZE + i))) {
            result = spirom_cmd_send(dev, SPIROM_CMD_ERASE_B64, EraseAddrStart, 0, NULL, &RetData);
            EraseAddrStart += SPIROM_BLK64_SIZE;
            i += SPIROM_BLK64_SIZE / SPIROM_SECTOR_SIZE;
        } else if(!(EraseAddrStart & SPIROM_BLK32_MASK) && (EraseSectorCnt >= (SPIROM_BLK32_SIZE / SPIROM_SECTOR_SIZE + i))) {
            result = spirom_cmd_send(dev, SPIROM_CMD_ERASE_B32, EraseAddrStart, 0, NULL, &RetData);
            EraseAddrStart += SPIROM_BLK32_SIZE;
            i += SPIROM_BLK32_SIZE / SPIROM_SECTOR_SIZE;
        } else {
            result = spirom_cmd_send(dev, SPIROM_CMD_ERASE, EraseAddrStart, 0, NULL, &RetData);
            EraseAddrStart += SPIROM_SECTOR_SIZE;
            i += 1;
        }
        if(result != 0) {
            printf("mxic_erase: rom erase fail\n");
            break;
        }

        /*-- get erase status --*/
        result = mxic_wr_rdy(dev);
        if(result) break;
//        EraseAddrStart += SPIROM_SECTOR_SIZE;
//        EraseSectorIndex++;
    }

    return result;
}

_EXT_RAM int mxic_erase_page(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize)
{
    unsigned int EraseAddrStart, EraseSize, EraseCnt, /*EraseSectorIndex,*/ i, j;
    unsigned int result, RetData, timeout = dev->timeout;

    EraseAddrStart = (FlashAddr / SPIROM_PAGE_SIZE) * SPIROM_PAGE_SIZE;
    EraseSize = (FlashAddr - EraseAddrStart) + DataSize;
    EraseCnt = (EraseSize + (SPIROM_PAGE_SIZE - 1)) / SPIROM_PAGE_SIZE;
//    EraseSectorIndex = (EraseAddrStart / SPIROM_PAGE_SIZE);

    for(i = 0; i < EraseCnt; ) {
        /*-- write enable --*/
        result = mxic_wr_en(dev);
        if(result) break;
        /*-- erase --*/
        result = spirom_cmd_send(dev, SPIROM_CMD_ERASE_PG, EraseAddrStart, 0, NULL, &RetData);
        EraseAddrStart += SPIROM_PAGE_SIZE;
        i += 1;
        if(result != 0) {
            printf("mxic_erase: rom erase fail\n");
            break;
        }

        /*-- get erase status --*/
        result = mxic_wr_rdy(dev);
        if(result) break;
//        EraseAddrStart += SPIROM_PAGE_SIZE;
//        EraseSectorIndex++;
    }

    return result;
}

_EXT_RAM int _mxic_program(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int* start, unsigned int DataSize)
{
    unsigned int result, RetData, *pdata = (unsigned int *)start; // k;
    unsigned int j, timeout = dev->timeout;

    unsigned int step_size, remain_size = DataSize;

    do {
        step_size = SPIROM_PAGE_SIZE - (FlashAddr & SPIROM_PAGE_MASK);
        step_size = (step_size > remain_size) ? remain_size : step_size;
        /*---------------------------*/
        /*-- PAGE PROGRAM procedure  */
        /*---------------------------*/
        /*-- write enable --*/
        result = mxic_wr_en(dev);
        if(result) break;

        // printf("send program cmd......\n");
        result = spirom_cmd_send(dev, SPIROM_CMD_PROGRAM, FlashAddr, step_size, pdata, &RetData);
        if(result != 0) {
            printf("mxic_program: (program) page %d fail\n", i);
            break;
        }
        /*-- ckeck completion --*/
        result = mxic_wr_rdy(dev);
        if(result) break;

        FlashAddr += step_size;
        pdata = (unsigned int *)((unsigned int)pdata + step_size);
        remain_size -= step_size;
    }while(remain_size);

    return result;
}

_EXT_RAM void mem_cpy(void *dst, void *src, int size)
{
    for(int i = 0; i < size; i++) {
        ((unsigned char *)dst)[i] = ((unsigned char *)src)[i];
    }
}

_EXT_RAM int mxic_program(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize)
{
    unsigned int result, RetData, FirstWrite, MidWrite, LastWrite, addr;
    unsigned char data[4] __attribute__((aligned(4)));
    unsigned char *pdata = start;

    do {
        //handle non 4 bytes aligned buffer and size
        FirstWrite = 4 - ((unsigned int)start & 0x3);
        if(FirstWrite == 4) {
            FirstWrite = 0;
        }
        if(DataSize > FirstWrite) {
            LastWrite = (DataSize - FirstWrite) & 0x3;
            MidWrite = DataSize - FirstWrite - LastWrite;
        } else {
            FirstWrite = DataSize;
            MidWrite = 0;
            LastWrite = 0;
        }
        addr = FlashAddr;
        if(FirstWrite) {
            //non 4 bytes aligned write
            mem_cpy(data, pdata, FirstWrite);
            result = _mxic_program(dev, addr, (unsigned int*)data, FirstWrite);
            if(result) break;
            pdata += FirstWrite;
            addr += FirstWrite;
        }
        if(MidWrite) {
            //4 bytes aligned write
            result = _mxic_program(dev, addr, (unsigned int*)pdata, MidWrite);
            if(result) break;
            pdata += MidWrite;
            addr += MidWrite;
        }
        if(LastWrite) {
            //non 4 bytes aligned write
            mem_cpy(data, pdata, LastWrite);
            result = _mxic_program(dev, addr, (unsigned int*)data, LastWrite);
        }
    } while(0);

    return result;
}

// TODO: modify this function to target-specific function - read flash
_EXT_RAM int _mxic_read(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int start, unsigned int DataSize)
{
    unsigned int result, RetData, CurrSize;
    /*-- SPIB_DCTRL_RCNT_MASK(0x1ff) --*/
    while(DataSize) {
        if(DataSize >= 0x200)
            CurrSize = 0x200;
        else
            CurrSize = DataSize;
        result = spirom_cmd_send(dev, SPIROM_CMD_READ, FlashAddr, CurrSize, (unsigned int*)start, &RetData);
        if(result != 0) {
            printf("Flash_Read: fail\n");
            break;
        }
        FlashAddr += CurrSize;
        start += CurrSize;
        DataSize -= CurrSize;
    }
    return result;
}

_EXT_RAM int mxic_read(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize)
{
    unsigned int result, FirstRead, MidRead, LastRead, addr;
    unsigned char data[4] __attribute__((aligned(4)));
    unsigned char *pdata = start;

    do {
        //handle non 4 bytes aligned buffer and size
        FirstRead = 4 - ((unsigned int)start & 0x3);
        if(FirstRead == 4) {
            FirstRead = 0;
        }
        if(DataSize > FirstRead) {
            LastRead = (DataSize - FirstRead) & 0x3;
            MidRead = DataSize - FirstRead - LastRead;
        } else {
            FirstRead = DataSize;
            MidRead = 0;
            LastRead = 0;
        }
        addr = FlashAddr;
        if(FirstRead) {
            //non 4 bytes aligned read
            result = _mxic_read(dev, addr, (unsigned int)data, FirstRead);
            if(result) break;
            mem_cpy(pdata, data, FirstRead);
            pdata += FirstRead;
            addr += FirstRead;
        }
        if(MidRead) {
            //4 bytes aligned read
            result = _mxic_read(dev, addr, (unsigned int)pdata, MidRead);
            if(result) break;
            pdata += MidRead;
            addr += MidRead;
        }
        if(LastRead) {
            //non 4 bytes aligned read
            result = _mxic_read(dev, addr, (unsigned int)data, LastRead);
            if(result) break;
            mem_cpy(pdata, data, LastRead);
        }
    } while(0);

    return result;
}

// TODO: modify this function to target-specific function - lock flash
_EXT_RAM int mxic_lock(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize)
{
#if INDIVIDUAL_BLK_PROTECT
#else
    unsigned int RetData = 0;

    spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &RetData);
    mxic_set_wrsr(dev, RetData | 0x3C);
    spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &RetData);
    printf("mxic_lock: status = %08x \n", RetData);
#endif
    return 0;
}

// TODO: modify this function to target-specific function - unlock flash
_EXT_RAM int mxic_unlock(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize)
{
#if INDIVIDUAL_BLK_PROTECT
#else
    unsigned int RetData = 0;

    spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &RetData);
    mxic_set_wrsr(dev, RetData & ~0x3C);
    spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &RetData);
    printf("mxic_unlock: status = %08x \n", RetData);
#endif
    return 0;
}

_EXT_RAM int mxic_set_wrsr(FLASH_DEV *dev, unsigned int uiStat)
{
    unsigned int result, RetData;

    do {
        result = mxic_wr_en(dev);
        if(result) break;

        /*-- set WRSR --*/
        result = spirom_cmd_send(dev, SPIROM_CMD_WRSR, uiStat, 0, NULL, &RetData);
        if(result) break;

        /*-- get status --*/
        result = mxic_wr_rdy(dev);
    } while(0);

    // printf("mxic_set_wrsr: uiStat=%x RetData=%x\n", uiStat, RetData);
    return result;
}

_EXT_RAM int mxic_qd_bit(FLASH_DEV *dev)
{
    unsigned int buf2;
    int result = 0;
    do {
        result = spirom_cmd_send(dev, SPIROM_CMD_RDST2, 0x0, 0, NULL, &buf2);
        if(result) break;

        if(buf2 & 0x2) {//if it is in QE mode, return 1, else return 0
            result = 1;
        } else {
            result = 0;
        }
    } while(0);

    return result;
}

_EXT_RAM int mxic_enable_qd(FLASH_DEV *dev)
{
    unsigned int buf1, buf2, buf_st12;
    int result = 0;
    do {
        result = spirom_cmd_send(dev, SPIROM_CMD_RDST2, 0x0, 0, NULL, &buf2);
        if(result) break;

        if(buf2 & 0x2) {//if it is in QE mode, return
            break;
        } else {
            buf2 |= 0x2;
            result = spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &buf_st12);
            if(result) break;

            buf_st12 |= (buf2 << 8);

            result = mxic_wr_en(dev);
            if(result) break;
            result = spirom_cmd_send(dev, SPIROM_CMD_WRSR, buf_st12, 1, NULL, &buf1);
            if(result) break;
            /*-- get enable status --*/
            result = mxic_wr_rdy(dev);
            if(result) break;

            result = spirom_cmd_send(dev, SPIROM_CMD_RDST2, 0x0, 0, NULL, &buf2);
            if(result) break;

            if(buf2 & 0x2) {//QE mode enabled
                break;
            } else {
                return -1;
            }
        }
    } while(0);

    return result;
}

_EXT_RAM int mxic_disable_qd(FLASH_DEV *dev)
{
    unsigned int buf1, buf2, buf_st12;
    int result = 0;
    do {
        result = spirom_cmd_send(dev, SPIROM_CMD_RDST2, 0x0, 0, NULL, &buf2);
        if(result) break;

        if(buf2 & 0x2) {//if it is in QE mode
            buf2 &= ~0x2;
            result = spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &buf_st12);
            if(result) break;

            buf_st12 |= (buf2 << 8);

            result = mxic_wr_en(dev);
            if(result) break;
            result = spirom_cmd_send(dev, SPIROM_CMD_WRSR, buf_st12, 1, NULL, &buf1);
            if(result) break;
            /*-- get enable status --*/
            result = mxic_wr_rdy(dev);
        }
    } while(0);

    return result;
}

_EXT_RAM int mxic_enable_dc(FLASH_DEV *dev)
{
    int result = 0;
    unsigned int buf1, buf2;

    do {
        result = spirom_cmd_send(dev, SPIROM_CMD_RDST3, 0x0, 0, NULL, &buf2);
        if(result) break;
        if(buf2 & 0x1) { //if DC bit is set
            break;
        } else {
            buf2 |= 0x1;
            result = mxic_wr_en(dev);
            if(result) break;

            result = spirom_cmd_send(dev, SPIROM_CMD_WRSR3, buf2, 0, NULL, &buf1);
            if(result) break;

            /*-- get enable status --*/
            result = mxic_wr_rdy(dev);
        }
    } while(0);

    return result;
}

_EXT_RAM int mxic_disable_dc(FLASH_DEV *dev)
{
    int result = 0;
    unsigned int buf1, buf2;

    do {
        result = spirom_cmd_send(dev, SPIROM_CMD_RDST3, 0x0, 0, NULL, &buf2);
        if(result) break;
        if(buf2 & 0x1) { //if DC bit is set
            buf2 &= ~0x1;
            result = mxic_wr_en(dev);
            if(result) break;

            result = spirom_cmd_send(dev, SPIROM_CMD_WRSR3, buf2, 0, NULL, &buf1);
            if(result) break;

            /*-- get enable status --*/
            result = mxic_wr_rdy(dev);
        }
    } while(0);

    return result;
}

_EXT_RAM int mxic_get_dc(FLASH_DEV *dev, unsigned char *dc)
{
    int result;
    unsigned int buf2;

    do {
        result = spirom_cmd_send(dev, SPIROM_CMD_RDST3, 0x0, 0, NULL, &buf2);
        if(result) break;
        *dc = buf2 & 0x1;
    } while(0);

    return result;
}

_EXT_RAM int mxic_security_program(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize)
{
    unsigned int result, RetData;

    /*-- write enable --*/
    result = mxic_wr_en(dev);
    if(result) return result;

    result = spirom_cmd_send(dev, SPIROM_CMD_SEC_PRGM, FlashAddr, DataSize, (unsigned int*)start, &RetData);
    if(result != 0) {
        printf("mxic_security_program: fail\n");
        return result;
    }
    /*-- ckeck completion --*/
    result = mxic_wr_rdy(dev);

    return result;
}

_EXT_RAM int mxic_security_erase(FLASH_DEV *dev, unsigned int FlashAddr)
{
    unsigned int result, RetData;

    /*-- write enable --*/
    result = mxic_wr_en(dev);
    if(result) return result;
    /*-- erase --*/
    result = spirom_cmd_send(dev, SPIROM_CMD_SEC_ERASE, FlashAddr, 0, NULL, &RetData);

    if(result != 0) {
        printf("mxic_security_erase: security erase fail\n");
        return result;
    }

    /*-- get erase status --*/
    result = mxic_wr_rdy(dev);

    return result;
}

_EXT_RAM int mxic_security_read(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize)
{
    unsigned int result, RetData;

    result = spirom_cmd_send(dev, SPIROM_CMD_SEC_READ, FlashAddr, DataSize, (unsigned int*)start, &RetData);
    if(result != 0) {
        printf("mxic_security_read: fail\n");
    }
    return result;
}

_EXT_RAM int mxic_deep_power_down(FLASH_DEV *dev)
{
    unsigned int result, RetData;
    result = spirom_cmd_send(dev, SPIROM_CMD_PD, 0x0, 0, NULL, &RetData);
    if(result != 0) {
        printf("mxic_deep_power_down: fail\n");
    }
    return result;
}

_EXT_RAM int mxic_release_deep_power_down(FLASH_DEV *dev)
{
    unsigned int result, RetData;
    result = spirom_cmd_send(dev, SPIROM_CMD_REL_PD, 0x0, 0, NULL, &RetData);
    if(result != 0) {
        printf("mxic_release_deep_power_down: fail\n");
    }
    return result;
}

_EXT_RAM int mxic_write_lock_bit(FLASH_DEV *dev, unsigned char value)
{
    unsigned int result;
    unsigned int buf1, buf2, buf_st12;

    do {
        result = spirom_cmd_send(dev, SPIROM_CMD_RDST2, 0x0, 0, NULL, &buf2);
        if(result) break;

        if(buf2 & value) {
            break;
        } else {
            buf2 |= value;
            result = spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &buf_st12);
            if(result) break;

            buf_st12 |= (buf2 << 8);

            result = mxic_wr_en(dev);
            if(result) break;
            result = spirom_cmd_send(dev, SPIROM_CMD_WRSR, buf_st12, 0, NULL, &buf1);
            if(result) break;

            result = spirom_cmd_send(dev, SPIROM_CMD_RDST2, 0x0, 0, NULL, &buf2);
            if(result) break;

            if(buf2 & value) {
                break;
            } else {
                return -1;
            }
        }
    } while(0);

    return result;
}


_EXT_RAM int mxic_read_uinque_id(FLASH_DEV *dev, unsigned char *buff)
{
    unsigned int result, RetData;
    result = spirom_cmd_send(dev, SPIROM_CMD_RUID, 0x0, 16, (unsigned int*)buff, &RetData);

    if(result != 0) {
        printf("mxic_read_uinque_id: fail\n");
    }
    return result;
}

_EXT_RAM static int mxic_id_rems(FLASH_DEV *dev, unsigned short *id_rems)
{
    unsigned int result, RetData;
    unsigned char data[8] __attribute__((aligned(4)));

    result = spirom_cmd_send(dev, SPIROM_CMD_REMSID, 0x0, 2, (unsigned int*)data, &RetData);
    if(result != 0) {
        printf("mxic_id_rems: fail\n");
        return result;
    }

    *id_rems = (data[1] << 8) | data[0];

    return 0;
}
